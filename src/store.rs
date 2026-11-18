use std::{
    collections::HashMap,
    sync::{Arc, Mutex},
};

use crate::{
    timestamp::Timestamp, version::{Key, TxnIdOrTimestamp, Value, Version},
};

#[derive(Debug)]
pub struct Store {
    /// Key is the index. Value is the version chain for that key.
    map: Vec<Mutex<Arc<Version>>>,
}

impl Store {
    #[must_use]
    pub fn new(values: impl IntoIterator<Item = Value>) -> Store {
        Store {
            map: values
                .into_iter()
                .enumerate()
                .map(|(key, value)| {
                    Mutex::new(Arc::new(Version {
                        key: key as u64,
                        value,
                        begin: Mutex::new(TxnIdOrTimestamp::Timestamp(Timestamp(0))),
                        end: Mutex::new(TxnIdOrTimestamp::Infinity),
                        previous: None,
                    }))
                })
                .collect(),
        }
    }

    pub fn get_latest_version(&self, key: Key) -> Option<Arc<Version>> {
        let index = usize::try_from(key).ok()?;
        let head = self.map.get(index)?;
        Some(Arc::clone(&head.lock().unwrap()))
    }
}
