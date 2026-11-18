use std::sync::{Arc, Mutex};

use crate::{timestamp::Timestamp, transaction::TxnId};

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum TxnIdOrTimestamp {
    Infinity,
    Timestamp(Timestamp),
    TxnId(TxnId),
}

#[derive(Debug)]
pub struct Version {
    pub key: u64,
    pub value: u64,
    pub begin: Mutex<TxnIdOrTimestamp>,
    pub end: Mutex<TxnIdOrTimestamp>,
    pub previous: Option<Arc<Version>>,
}
