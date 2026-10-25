#include "../dbusimage.hpp"
#include <climits>

#include <qbytearray.h>
#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qdbusargument.h>
#include <qdbusconnection.h>
#include <qdbusmessage.h>
#include <qdbuspendingreply.h>
#include <qeventloop.h>
#include <qhashfunctions.h>
#include <qlogging.h>
#include <qobject.h>
#include <qsize.h>
#include <qstring.h>
#include <qtdbusglobal.h>
#include <qtest.h>
#include <qtestcase.h>
#include <qtmetamacros.h>
#include <qvariant.h>

using namespace qs::service::notifications;

namespace {

class ImageEcho: public QObject {
	Q_OBJECT;
	Q_CLASSINFO("D-Bus Interface", "org.quickshell.ImageTest");

public slots:
	static QVariantMap echo(const QVariantMap& values) { return values; }
};

class TestDBusImage: public QObject {
	Q_OBJECT;

private:
	ImageEcho echo;

	static QDBusArgument wireImage(
	    int width,
	    int height,
	    int stride,
	    bool alpha,
	    int bits,
	    int channels,
	    const QByteArray& bytes
	) {
		auto argument = QDBusArgument();
		argument.beginStructure();
		argument << width << height << stride << alpha << bits << channels << bytes;
		argument.endStructure();
		return argument;
	}

	static QDBusArgument receive(const QDBusArgument& argument) {
		auto const bus = QDBusConnection::sessionBus();
		auto message = QDBusMessage::createMethodCall(
		    bus.baseService(),
		    "/image",
		    "org.quickshell.ImageTest",
		    "echo"
		);
		message << QVariantMap {{"image", QVariant::fromValue(argument)}};
		auto const client =
		    QDBusConnection::connectToBus(QDBusConnection::SessionBus, "image-test-client");
		auto const reply = QDBusPendingReply<QVariantMap>(client.asyncCall(message));
		// A blocking call to our own thread cannot dispatch the echo method.
		while (!reply.isFinished()) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		if (reply.isError()) qFatal("D-Bus echo failed: %s", qPrintable(reply.error().message()));
		return reply.value().value("image").value<QDBusArgument>();
	}

private slots:
	void initTestCase() {
		auto bus = QDBusConnection::sessionBus();
		QVERIFY(bus.isConnected());
		QVERIFY(bus.registerObject("/image", &this->echo, QDBusConnection::ExportAllSlots));
	}

	static void pixels_data() { // NOLINT(readability-identifier-naming)
		QTest::addColumn<int>("stride");
		QTest::addColumn<bool>("alpha");
		QTest::addColumn<bool>("lastRowPadding");
		QTest::newRow("packed-rgb") << 3 << false << false;
		QTest::newRow("padded-rgb") << 4 << false << true;
		QTest::newRow("gdk-last-row-unpadded") << 4 << false << false;
		QTest::newRow("larger-row-padding") << 8 << false << false;
		QTest::newRow("rgba") << 4 << true << false;
	}

	static void pixels() {
		QFETCH(const int, stride);
		QFETCH(const bool, alpha);
		QFETCH(const bool, lastRowPadding);
		auto const rowBytes = alpha ? 4 : 3;
		auto bytes = QByteArray(stride + (lastRowPadding ? stride : rowBytes), '\0');
		bytes.replace(0, rowBytes, QByteArray::fromHex(alpha ? "ff0000ff" : "ff0000"));
		bytes.replace(stride, rowBytes, QByteArray::fromHex(alpha ? "00ff0080" : "00ff00"));

		auto image = DBusNotificationImage();
		receive(wireImage(1, 2, stride, alpha, 8, rowBytes, bytes)) >> image;
		auto const decoded = image.createImage();
		QCOMPARE(decoded.size(), QSize(1, 2));
		QCOMPARE(decoded.pixelColor(0, 0), QColor(255, 0, 0, 255));
		QCOMPARE(decoded.pixelColor(0, 1), QColor(0, 255, 0, alpha ? 128 : 255));
		// Mutating and then freeing the notification must not affect cached pixels.
		image.data.fill('\0');
		image = {};
		QCOMPARE(decoded.pixelColor(0, 0), QColor(255, 0, 0, 255));
	}

	static void invalid_data() { // NOLINT(readability-identifier-naming)
		QTest::addColumn<QDBusArgument>("argument");
		QTest::newRow("short-buffer") << wireImage(2, 2, 8, true, 8, 4, QByteArray(15, '\0'));
		QTest::newRow("negative-width") << wireImage(-1, 1, 4, false, 8, 3, QByteArray(4, '\0'));
		QTest::newRow("zero-height") << wireImage(1, 0, 4, false, 8, 3, QByteArray(4, '\0'));
		QTest::newRow("negative-stride") << wireImage(1, 1, -4, false, 8, 3, QByteArray(4, '\0'));
		QTest::newRow("small-stride") << wireImage(2, 2, 3, false, 8, 3, QByteArray(12, '\0'));
		QTest::newRow("bad-depth") << wireImage(1, 1, 4, true, 16, 4, QByteArray(4, '\0'));
		QTest::newRow("bad-channels") << wireImage(1, 1, 4, true, 8, 3, QByteArray(4, '\0'));
		QTest::newRow("size-overflow") << wireImage(1, 16384, 262144, true, 8, 4, QByteArray(4, '\0'));
		QTest::newRow("width-overflow") << wireImage(INT_MAX, 1, 4, true, 8, 4, QByteArray(4, '\0'));
		auto malformed = QDBusArgument();
		malformed.beginStructure();
		malformed << 1 << 1 << 4 << QByteArray(4, '\0');
		malformed.endStructure();
		QTest::newRow("wrong-signature") << malformed;
	}

	static void invalid() {
		QFETCH(const QDBusArgument, argument);
		// An invalid replacement also clears any old image.
		auto image = DBusNotificationImage();
		image.data = QByteArray(4, '\0');
		receive(argument) >> image;
		QVERIFY(image.data.isEmpty());
		QVERIFY(image.createImage().isNull());
	}
};

} // namespace

QTEST_GUILESS_MAIN(TestDBusImage);
#include "dbusimage.moc"
