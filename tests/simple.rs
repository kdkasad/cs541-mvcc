use std::sync::Arc;

use cs541_mvcc::{store::Store, transaction::TransactionManager, version::Version};

#[allow(dead_code)]
fn print_version_chain(head: Option<Arc<Version>>) {
    let mut current = head;
    println!("Start version chain");
    while let Some(version) = current {
        println!("begin = {:?}, end = {:?}, key = {}, value = {}", version.begin.lock().unwrap(), version.end.lock().unwrap(), version.key, version.value);
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
