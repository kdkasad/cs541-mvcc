use std::{collections::HashMap, sync::{Arc, Mutex}};

use crate::{store::Store, timestamp::Timestamp, version::{Key, Value, Version}};

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
pub struct TransactionManager {
    next_id: u64,
    next_timestamp: u64,
    transactions: HashMap<TxnId, Arc<TxnMetadata>>,
}

impl TransactionManager {
    pub fn new() -> Mutex<Self> {
        Mutex::new(Self {
            next_id: 1,
            next_timestamp: 1,
            transactions: HashMap::new(),
        })
    }

    pub fn begin(&mut self) -> Transaction {
        let id = TxnId(self.next_id);
        self.next_id += 1;
        let begin_ts = Timestamp(self.next_timestamp);
        self.next_timestamp += 1;
        let metadata = Arc::new(TxnMetadata {
            id,
            begin_ts,
            status: Mutex::new(TxnStatus::Active),
        });
        self.transactions.insert(id, Arc::clone(&metadata));
        Transaction {
            metadata,
            read_set: Vec::new(),
            write_set: Vec::new(),
        }
    }

    pub fn get(&self, id: TxnId) -> Option<Arc<TxnMetadata>> {
        self.transactions.get(&id).cloned()
    }
}

#[derive(Debug)]
pub struct Transaction {
    metadata: Arc<TxnMetadata>,
    read_set: Vec<Arc<Version>>,
    write_set: Vec<Arc<Version>>,
}

impl Transaction {
    pub fn optimistic_read(&mut self, store: &Store, key: Key) -> Option<Value> {
        let mut current = store.get_latest_version(key);
        let read_ts = self.metadata.begin_ts;
        while let Some(version) = current {
            if version.is_visible_at(read_ts) {
                let value = version.value;
                self.read_set.push(version);
                return Some(value);
            }
            current = version.previous.clone();
        }
        None
    }
}
