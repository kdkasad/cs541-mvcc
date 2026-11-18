use std::{
    collections::HashMap,
    sync::{Arc, Mutex},
};

use crate::{
    store::Store,
    timestamp::Timestamp,
    version::{Key, Value, Version},
};

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TxnId(pub u64);

#[derive(Copy, Clone, Debug)]
pub enum TxnStatus {
    Active,
    Aborted,
    Preparing { end_ts: Timestamp },
    Committed { end_ts: Timestamp },
}

#[derive(Debug)]
pub struct TxnMetadata {
    pub id: TxnId,
    pub begin_ts: Timestamp,
    status: Mutex<TxnStatus>,
}

impl TxnMetadata {
    #[must_use]
    pub fn status(&self) -> TxnStatus {
        *self.status.lock().unwrap()
    }
}

#[derive(Debug)]
struct TransactionManagerInner {
    next_id: u64,
    next_timestamp: u64,
    transactions: HashMap<TxnId, Arc<TxnMetadata>>,
}

#[derive(Debug)]
pub struct TransactionManager {
    inner: Mutex<TransactionManagerInner>,
}

impl TransactionManager {
    pub fn new() -> Arc<Self> {
        Arc::new(Self {
            inner: Mutex::new(TransactionManagerInner {
                next_id: 1,
                next_timestamp: 1,
                transactions: HashMap::new(),
            }),
        })
    }

    pub fn begin(self: &Arc<Self>) -> Transaction {
        let mut inner = self.inner.lock().unwrap();
        let id = TxnId(inner.next_id);
        inner.next_id += 1;
        let begin_ts = Timestamp(inner.next_timestamp);
        inner.next_timestamp += 1;
        let metadata = Arc::new(TxnMetadata {
            id,
            begin_ts,
            status: Mutex::new(TxnStatus::Active),
        });
        inner.transactions.insert(id, Arc::clone(&metadata));
        Transaction {
            metadata,
            read_set: Vec::new(),
            write_set: Vec::new(),
            manager: Arc::clone(self),
        }
    }

    pub fn get(&self, id: TxnId) -> Option<Arc<TxnMetadata>> {
        self.inner.lock().unwrap().transactions.get(&id).cloned()
    }
}

#[derive(Debug)]
pub struct WriteEntry {
    pub old: Arc<Version>,
    pub new: Arc<Version>,
}

#[derive(Debug)]
pub struct Transaction {
    metadata: Arc<TxnMetadata>,
    read_set: Vec<Arc<Version>>,
    write_set: Vec<WriteEntry>,

    manager: Arc<TransactionManager>,
}

#[derive(Debug)]
pub enum WriteError {
    /// The given key is not in the range covered by the store.
    KeyOutOfRange,
    /// A write-write conflict happened.
    /// Another transaction updated the same key during our transaction.
    Conflict,
    /// This key has already been written in this transaction.
    AlreadyWritten,
}

#[derive(Debug)]
pub enum CommitError {}

impl Transaction {
    /// Panic if called on an aborted/committed transaction
    fn panic_if_not_active(&self) {
        assert!(matches!(self.metadata.status(), TxnStatus::Active));
    }

    pub fn optimistic_read(&mut self, store: &Store, key: Key) -> Option<Value> {
        self.panic_if_not_active();
        self.optimistic_read_impl(store, key)
    }

    fn optimistic_read_impl(&mut self, store: &Store, key: Key) -> Option<Value> {
        let mut current = store.get_latest_version(key);
        while let Some(version) = current {
            if version.is_visible_at(self.metadata.begin_ts, self.metadata.id, &self.manager) {
                let value = version.value;
                self.read_set.push(version);
                return Some(value);
            }
            current = version.previous.lock().unwrap().clone();
        }
        None
    }

    pub fn optimistic_scan(&mut self, store: &Store) -> Option<Vec<Value>> {
        self.panic_if_not_active();
        store
            .key_range()
            .map(|key| self.optimistic_read_impl(store, key))
            .collect()
    }

    pub fn write(&mut self, store: &Store, key: Key, value: Value) -> Result<(), WriteError> {
        self.panic_if_not_active();
        if self.write_set.iter().any(|entry| entry.new.key == key) {
            return Err(WriteError::AlreadyWritten);
        }
        let entry = store.install_update(key, value, self.metadata.id, self.metadata.begin_ts)?;
        self.write_set.push(entry);
        Ok(())
    }
}
