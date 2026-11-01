#include <memory>
#include <utility>

#include <polkit/polkit.h>
#include <qhashfunctions.h>
#include <qlist.h>
#include <qobject.h>
#include <qsignalspy.h>
#include <qtest.h>
#include <qtestcase.h>
#include <qtmetamacros.h>
#include <unistd.h>

#include "../flow.hpp"
#include "../gobjectref.hpp"
#include "../identity.hpp"
#include "../listener.hpp"
#include "../session.hpp"

// Test-session counters are shared with the fake Session methods.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
namespace {
int sessionsStarted = 0;
int sessionsCancelled = 0;
int responses = 0;
int requestsCompleted = 0;
int requestsCancelled = 0;
bool deferCancellation = false;
} // namespace
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// No PAM helper or authority is contacted: drive the real AuthFlow with a
// synchronous session, including completion from inside cancel().
// Keep the fake implementations compatible with the real instance methods.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
namespace qs::service::polkit {
Session::Session(PolkitIdentity* /*unused*/, const QString& /*unused*/, QObject* parent)
    : QObject(parent) {}
Session::~Session() = default;
void Session::initiate() { ++sessionsStarted; }
void Session::cancel() {
	++sessionsCancelled;
	if (!deferCancellation) emit this->completed(false);
}
void Session::respond(const QString& /*unused*/) { ++responses; }
void AuthRequest::complete() { ++requestsCompleted; }
void AuthRequest::cancel(const QString& /*unused*/) { ++requestsCancelled; }
} // namespace qs::service::polkit
// NOLINTEND(readability-convert-member-functions-to-static)

using namespace qs::service::polkit;

// NOLINTNEXTLINE(misc-use-internal-linkage)
class TestAuthFlow: public QObject {
	Q_OBJECT;

	static AuthFlow* createFlow() {
		auto* request = new AuthRequest {};
		auto const identity =
		    GObjectRef<PolkitIdentity>(polkit_unix_user_new(static_cast<int>(getuid())));
		auto identities = QList<Identity*> {Identity::fromPolkitIdentity(identity)};
		return new AuthFlow(request, std::move(identities));
	}

private slots:
	static void init() {
		sessionsStarted = 0;
		sessionsCancelled = 0;
		responses = 0;
		requestsCompleted = 0;
		requestsCancelled = 0;
		deferCancellation = false;
	}

	static void finishedFlow_data() { // NOLINT(readability-identifier-naming)
		QTest::addColumn<int>("finish");
		QTest::addColumn<int>("action");
		for (int finish = 0; finish < 3; finish++) {
			for (int action = 0; action < 3; action++) {
				QTest::newRow(qPrintable(QString("finish-%1-action-%2").arg(finish).arg(action)))
				    << finish << action;
			}
		}
	}

	static void finishedFlow() {
		QFETCH(const int, finish);
		QFETCH(const int, action);
		auto flow = std::unique_ptr<AuthFlow>(createFlow());
		auto* session = flow->findChild<Session*>();
		QVERIFY(session);
		QSignalSpy const completed(flow.get(), &AuthFlow::isCompletedChanged);
		QSignalSpy const cancelled(flow.get(), &AuthFlow::authenticationRequestCancelled);
		if (finish == 0) session->completed(true);
		else if (finish == 1) flow->cancelFromAgent();
		else flow->cancelAuthenticationRequest();
		QVERIFY(flow->isCompleted().value());
		QCOMPARE(flow->isSuccessful().value(), finish == 0);
		QCOMPARE(completed.count(), 1);

		if (action == 0) flow->submit("late response");
		else if (action == 1) flow->cancelFromAgent();
		else flow->cancelAuthenticationRequest();

		QCOMPARE(responses, 0);
		QCOMPARE(sessionsCancelled, finish == 0 ? 0 : 1);
		QCOMPARE(requestsCompleted, finish == 0 ? 1 : 0);
		QCOMPARE(requestsCancelled, finish == 0 ? 0 : 1);
		QCOMPARE(cancelled.count(), finish == 1 ? 1 : 0);
		QCOMPARE(completed.count(), 1);
		QCOMPARE(flow->isSuccessful().value(), finish == 0);
	}

	static void pendingCancellation_data() { // NOLINT(readability-identifier-naming)
		QTest::addColumn<bool>("fromAgent");
		QTest::newRow("agent") << true;
		QTest::newRow("user") << false;
	}

	static void pendingCancellation() {
		QFETCH(const bool, fromAgent);
		deferCancellation = true;
		auto flow = std::unique_ptr<AuthFlow>(createFlow());
		if (fromAgent) flow->cancelFromAgent();
		else flow->cancelAuthenticationRequest();
		QVERIFY(!flow->isCompleted().value());
		flow->submit("late response");
		flow->cancelFromAgent();
		flow->cancelAuthenticationRequest();
		QCOMPARE(responses, 0);
		QCOMPARE(sessionsCancelled, 1);
		QCOMPARE(requestsCancelled, 1);
		flow->findChild<Session*>()->completed(false);
		QVERIFY(flow->isCompleted().value());
	}

	static void activeFlowStillAcceptsResponsesAndRetries() {
		auto flow = std::unique_ptr<AuthFlow>(createFlow());
		flow->submit("response");
		QCOMPARE(responses, 1);
		flow->findChild<Session*>()->completed(false);
		QCOMPARE(sessionsStarted, 2);
		QVERIFY(!flow->isCompleted().value());
		flow->submit("retry");
		QCOMPARE(responses, 2);
	}
};

QTEST_GUILESS_MAIN(TestAuthFlow);
#include "flow.moc"
