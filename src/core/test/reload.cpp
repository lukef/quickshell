#include "../reload.hpp"

#include <QtTest>

class Probe: public Reloadable {
public:
	QObject* previous = nullptr;
	void onReload(QObject* old) override { previous = old; }
};
class ReloadTest: public QObject {
	Q_OBJECT
private slots:
	void namedChildrenSurviveInsertion() {
		ReloadPropagator before, after;
		auto append = [](ReloadPropagator& scope, QObject* child) {
			auto list = scope.data();
			list.append(&list, child);
		};
		auto* oldLock = new Probe;
		oldLock->mReloadableId = "lock";
		append(before, oldLock);
		auto* extra = new Probe;
		append(after, extra);
		auto* lock = new Probe;
		lock->mReloadableId = "lock";
		append(after, lock);
		after.reload(&before);
		QCOMPARE(lock->previous, oldLock);
		QVERIFY(!extra->previous);
	}
	void newlyNamedChildRetainsLegacyState() {
		ReloadPropagator before, after;
		auto a = before.data(), b = after.data();
		auto* previous = new Probe;
		a.append(&a, previous);
		auto* current = new Probe;
		current->mReloadableId = "lock";
		b.append(&b, current);
		after.reload(&before);
		QCOMPARE(current->previous, previous);
	}
	void differentNamedChildrenNeverShareState() {
		ReloadPropagator before, after;
		auto a = before.data(), b = after.data();
		auto* previous = new Probe;
		previous->mReloadableId = "lock";
		a.append(&a, previous);
		auto* current = new Probe;
		current->mReloadableId = "other";
		b.append(&b, current);
		after.reload(&before);
		QVERIFY(!current->previous);
	}
	void plainObjectsPropagateNamedChildren() {
		ReloadPropagator before, after;
		auto append = [](ReloadPropagator& scope, QObject* child) {
			auto list = scope.data();
			list.append(&list, child);
		};
		auto* oldWrapper = new QObject;
		append(before, oldWrapper);
		auto* oldLock = new Probe;
		oldLock->setParent(oldWrapper);
		oldLock->mReloadableId = "lock";
		auto* wrapper = new QObject;
		append(after, wrapper);
		auto* lock = new Probe;
		lock->setParent(wrapper);
		lock->mReloadableId = "lock";
		after.reload(&before);
		QCOMPARE(lock->previous, oldLock);
	}
};
QTEST_GUILESS_MAIN(ReloadTest)
#include "reload.moc"
