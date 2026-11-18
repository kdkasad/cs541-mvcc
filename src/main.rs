use cs541_mvcc::{store::Store, transaction::TransactionManager};

fn main() {
    let store = Store::new(vec![1, 2, 3, 4, 5]);
    let manager = TransactionManager::new();
    let mut t1 = manager.lock().unwrap().begin();
    dbg!(t1.optimistic_read(&store, 0));
}
