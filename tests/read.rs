use cs541_mvcc::{store::Store, transaction::TransactionManager};

#[test]
fn simple_read_existing() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.lock().unwrap().begin();
    assert_eq!(Some(1), t1.optimistic_read(&store, 0));
}

#[test]
fn simple_read_missing() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.lock().unwrap().begin();
    assert_eq!(None, t1.optimistic_read(&store, 20));
}
