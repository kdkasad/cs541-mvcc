use std::sync::{Arc, Mutex};

use crate::{
    timestamp::Timestamp,
    transaction::{TransactionManager, TxnId, TxnStatus},
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
    pub previous: Mutex<Option<Arc<Version>>>,
}

impl Version {
    pub fn is_visible_at(
        &self,
        ts: Timestamp,
        reader: TxnId,
        txn_manager: &TransactionManager,
    ) -> bool {
        // Hold the locks so we don't hit a race condition between reading the
        // fields and checking the manager for the transactions they reference
        let begin = self.begin.lock().unwrap();
        let end = self.end.lock().unwrap();

        let txn_status = |id: TxnId| {
            txn_manager
                .get(id)
                .expect("version references transaction that doesn't exist")
                .status()
        };

        let visible_according_to_begin = match *begin {
            TxnIdOrTimestamp::Infinity => false,
            TxnIdOrTimestamp::Timestamp(begin_ts) => begin_ts <= ts,
            TxnIdOrTimestamp::TxnId(writer) if writer == reader => {
                *end == TxnIdOrTimestamp::Infinity
            }
            TxnIdOrTimestamp::TxnId(writer) => match txn_status(writer) {
                TxnStatus::Active | TxnStatus::Aborted => false,
                TxnStatus::Committed { end_ts } => end_ts <= ts,
                TxnStatus::Preparing { .. } => {
                    eprintln!("Making version invisible because the writer is preparing");
                    false
                }
            },
        };
        if !visible_according_to_begin {
            return false;
        }

        match *end {
            TxnIdOrTimestamp::Infinity => true,
            TxnIdOrTimestamp::Timestamp(end_ts) => ts < end_ts,
            TxnIdOrTimestamp::TxnId(writer) => match txn_status(writer) {
                // See errata for this case. Larson paper is wrong.
                TxnStatus::Active => writer != reader,
                TxnStatus::Aborted => true,
                TxnStatus::Committed { end_ts } => ts < end_ts,
                TxnStatus::Preparing { .. } => {
                    eprintln!("Making version invisible because the writer is preparing");
                    false
                }
            },
        }
    }
}
