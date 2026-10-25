#include "dbusimage.hpp"
#include <cstring>

#include <qbytearrayview.h>
#include <qdbusargument.h>
#include <qimage.h>
#include <qloggingcategory.h>
#include <qsize.h>
#include <qtypes.h>

#include "../../core/logcat.hpp"

namespace qs::service::notifications {

// NOLINTNEXTLINE(misc-use-internal-linkage)
QS_DECLARE_LOGGING_CATEGORY(logNotifications); // server.cpp

QImage DBusNotificationImage::createImage() const {
	if (this->data.isEmpty()) return {};

	const auto format = this->hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
	auto image = QImage(this->width, this->height, format);
	if (image.isNull()) return image;

	// The last row need not include padding. Copy only its pixels, and give
	// cached images storage independent of notification updates and closes.
	const auto rowBytes = static_cast<qsizetype>(this->width) * (this->hasAlpha ? 4 : 3);
	for (auto row = 0; row < this->height; row++) {
		std::memcpy(
		    image.scanLine(row),
		    QByteArrayView(this->data)
		        .sliced(static_cast<qsizetype>(row) * this->rowstride, rowBytes)
		        .data(),
		    rowBytes
		);
	}

	return image;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, DBusNotificationImage& pixmap) {
	pixmap = {};
	if (argument.currentSignature() != "(iiibiiay)") {
		qCWarning(logNotifications) << "Unable to parse pixmap with signature"
		                            << argument.currentSignature();
		return argument;
	}

	argument.beginStructure();
	argument >> pixmap.width;
	argument >> pixmap.height;
	argument >> pixmap.rowstride;
	argument >> pixmap.hasAlpha;
	const auto sampleBits = qdbus_cast<qint32>(argument);
	const auto channels = qdbus_cast<qint32>(argument);
	argument >> pixmap.data;
	argument.endStructure();

	const auto expectedChannels = pixmap.hasAlpha ? 4 : 3;
	const auto rowBytes = static_cast<qint64>(pixmap.width) * expectedChannels;
	const auto requiredBytes =
	    static_cast<qint64>(pixmap.rowstride) * (static_cast<qint64>(pixmap.height) - 1) + rowBytes;
	if (sampleBits != 8) {
		qCWarning(logNotifications) << "Unable to parse pixmap as sample count is incorrect. Got"
		                            << sampleBits << "expected" << 8;
	} else if (channels != expectedChannels) {
		qCWarning(logNotifications) << "Unable to parse pixmap as channel count is incorrect."
		                            << "Got " << channels << "expected" << (pixmap.hasAlpha ? 4 : 3);
	} else if (pixmap.width <= 0 || pixmap.height <= 0) {
		qCWarning(logNotifications) << "Unable to parse pixmap with non-positive dimensions";
	} else if (pixmap.rowstride < rowBytes) {
		qCWarning(logNotifications) << "Unable to parse pixmap as rowstride is incorrect. Got"
		                            << pixmap.rowstride << "expected at least" << rowBytes;
	} else if (pixmap.data.size() < requiredBytes) {
		qCWarning(logNotifications) << "Unable to parse pixmap as its pixel data is too short";
	} else {
		return argument;
	}

	pixmap = {};
	return argument;
}

const QDBusArgument& operator<<(QDBusArgument& argument, const DBusNotificationImage& pixmap) {
	argument.beginStructure();
	argument << pixmap.width;
	argument << pixmap.height;
	argument << pixmap.rowstride;
	argument << pixmap.hasAlpha;
	argument << 8;
	argument << (pixmap.hasAlpha ? 4 : 3);
	argument << pixmap.data;
	argument.endStructure();
	return argument;
}

QImage
NotificationImage::requestImage(const QString& /*unused*/, QSize* size, const QSize& /*unused*/) {
	auto image = this->image.createImage();

	if (size != nullptr) *size = image.size();
	return image;
}

} // namespace qs::service::notifications
