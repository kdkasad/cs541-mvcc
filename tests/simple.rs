use std::sync::Arc;

use cs541_mvcc::{
    store::Store,
    transaction::{CommitError, TransactionManager, WriteError},
    version::Version,
};

#[allow(dead_code)]
fn print_version_chain(head: Option<Arc<Version>>) {
    let mut current = head;
    println!("Start version chain");
    while let Some(version) = current {
        println!(
            "begin = {:?}, end = {:?}, key = {}, value = {}",
            version.begin.lock().unwrap(),
            version.end.lock().unwrap(),
            version.key,
            version.value
        );
        current = version.previous.lock().unwrap().clone();
    }
    println!("End version chain");
}

#[test]
fn simple_read_existing() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
}

#[test]
fn simple_read_missing() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    assert_eq!(None, t1.optimistic_read(&store, 20));
}

#[test]
fn read_ignores_earlier_uncommitted_write() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    t1.write(&store, 0, 2).unwrap();
    let mut t2 = manager.begin();
    assert_eq!(Some(2), t1.optimistic_read(&store, 0));
    assert_eq!(Some(1), t2.optimistic_read(&store, 0));
}

// This test is different from the above because the writer transaction is
// started after the reader
#[test]
fn read_ignores_later_uncommitted_write() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    let mut t2 = manager.begin();
    t2.write(&store, 0, 2).unwrap();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
    assert_eq!(Some(2), t2.optimistic_read(&store, 0));
}

#[test]
fn simple_scan() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    assert_eq!(Some(vec![1, 2, 3, 4, 5]), t1.optimistic_scan(&store));
    t1.write(&store, 0, 2).unwrap();
    assert_eq!(Some(vec![2, 2, 3, 4, 5]), t1.optimistic_scan(&store));
}

#[test]
fn read_after_commit() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    t1.write(&store, 0, 2).unwrap();
    t1.optimistic_commit(&store).unwrap();
    let mut t2 = manager.begin();
    assert_eq!(Some(2), t2.optimistic_read(&store, 0));
}

#[test]
fn read_before_and_after_commit() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    let mut t2 = manager.begin();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
    t2.write(&store, 0, 2).unwrap();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
    t2.optimistic_commit(&store).unwrap();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
}

#[test]
fn conflicting_writes_abort_second_writer() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    let mut t2 = manager.begin();
    t1.write(&store, 0, 2).unwrap();
    // Larson et al. section 2.6:
    // > This is a write-write conflict. We follow the
    // > first-writer-wins rule and force transaction T to abort.
    assert_eq!(Err(WriteError::Conflict), t2.write(&store, 0, 3));
    assert_eq!(Some(2), t1.optimistic_read(&store, 0));
    assert_eq!(Some(1), t2.optimistic_read(&store, 0));
    t1.optimistic_commit(&store).unwrap();
    assert_eq!(
        Err(CommitError::NotSerializable),
        t2.optimistic_commit(&store)
    );
    let mut t3 = manager.begin();
    assert_eq!(Some(2), t3.optimistic_read(&store, 0));
}

#[test]
fn aborted_write_is_ignored() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    t1.write(&store, 0, 2).unwrap();
    assert_eq!(Some(2), t1.optimistic_read(&store, 0));
    t1.abort(&store);
    let mut t2 = manager.begin();
    assert_eq!(Some(1), t2.optimistic_read(&store, 0));
}

#[test]
fn write_after_aborted_write_succeeds() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    t1.write(&store, 0, 2).unwrap();
    assert_eq!(Some(2), t1.optimistic_read(&store, 0));
    t1.abort(&store);
    let mut t2 = manager.begin();
    assert_eq!(Some(1), t2.optimistic_read(&store, 0));
    t2.write(&store, 0, 3).unwrap();
    assert_eq!(Some(3), t2.optimistic_read(&store, 0));
    t2.optimistic_commit(&store).unwrap();
    let mut t3 = manager.begin();
    assert_eq!(Some(3), t3.optimistic_read(&store, 0));
}

#[test]
fn non_repeatable_read_is_aborted() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    let mut t2 = manager.begin();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
    t2.write(&store, 0, 2).unwrap();
    t2.optimistic_commit(&store).unwrap();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
    assert_eq!(
        Err(CommitError::NotSerializable),
        t1.optimistic_commit(&store)
    );
}

#[test]
fn version_created_and_invalidated_within_read_transaction() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.begin();
    let mut t2 = manager.begin();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
    // If committed, this write would invalidate t1
    t2.write(&store, 0, 2).unwrap();
    assert_eq!(Some(2), t2.optimistic_read(&store, 0));
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
    t2.abort(&store);
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
    // Because t2 aborted, t1 should commit successfully
    t1.optimistic_commit(&store).unwrap();
}
