use std::sync::{Arc, Mutex};

use crate::{
    timestamp::Timestamp,
    transaction::{TxnId, TxnMetadata},
    version::TxnIdOrTimestamp::Infinity,
};

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum TxnIdOrTimestamp {
    Infinity,
    Timestamp(Timestamp),
    TxnId(TxnId),
}

pub type Key = u64;
pub type Value = u64;

#[derive(Debug)]
pub struct Version {
    pub key: Key,
    pub value: Value,
    pub begin: Mutex<TxnIdOrTimestamp>,
    pub end: Mutex<TxnIdOrTimestamp>,
    pub previous: Option<Arc<Version>>,
}

impl Version {
    pub fn is_visible_at(&self, ts: Timestamp) -> bool {
        let begin = *self.begin.lock().unwrap();
        let end = *self.end.lock().unwrap();
        match (begin, end) {
            (TxnIdOrTimestamp::Timestamp(begin), TxnIdOrTimestamp::Timestamp(end)) => {
                begin <= ts && ts < end
            }

            (TxnIdOrTimestamp::Timestamp(begin), TxnIdOrTimestamp::Infinity) => begin <= ts,

            _ => todo!("handle case where begin/end is a transaction id"),
        }
    }
}
