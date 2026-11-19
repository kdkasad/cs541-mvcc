use std::{
    ops::Range,
    sync::{Arc, Mutex},
};

use crate::{
    timestamp::Timestamp,
    transaction::{TxnId, WriteEntry, WriteError},
    version::{Key, TxnIdOrTimestamp, Value, Version},
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
                        previous: Mutex::new(None),
                    }))
                })
                .collect(),
        }
    }

    pub fn key_range(&self) -> Range<Key> {
        0..(self.map.len() as u64)
    }

    pub fn get_latest_version(&self, key: Key) -> Option<Arc<Version>> {
        let index: usize = usize::try_from(key).ok()?;
        let head = self.map.get(index)?;
        Some(Arc::clone(&head.lock().unwrap()))
    }

    pub(crate) fn install_update(
        &self,
        key: Key,
        value: Value,
        writer: TxnId,
        read_ts: Timestamp,
    ) -> Result<WriteEntry, WriteError> {
        let index = usize::try_from(key).map_err(|_| WriteError::KeyOutOfRange)?;
        let slot = self.map.get(index).ok_or(WriteError::KeyOutOfRange)?;
        let mut head = slot.lock().unwrap();
        let old = Arc::clone(&head);
        let new = Arc::new(Version {
            key,
            value,
            begin: Mutex::new(TxnIdOrTimestamp::TxnId(writer)),
            end: Mutex::new(TxnIdOrTimestamp::Infinity),
            previous: Mutex::new(Some(Arc::clone(&old))),
        });

        {
            let begin = old.begin.lock().unwrap();
            let mut end = old.end.lock().unwrap();
            match *begin {
                TxnIdOrTimestamp::Timestamp(ts) => {
                    if ts > read_ts {
                        return Err(WriteError::Conflict);
                    }
                }
                TxnIdOrTimestamp::TxnId(_) => {
                    return Err(WriteError::Conflict);
                }
                TxnIdOrTimestamp::Infinity => panic!("begin should not be infinity"),
            }

            if *end != TxnIdOrTimestamp::Infinity {
                return Err(WriteError::Conflict);
            }

            *end = TxnIdOrTimestamp::TxnId(writer);
            *head = Arc::clone(&new);
        }
        Ok(WriteEntry { old, new })
    }

    pub fn remove_head(&self, version: Arc<Version>) {
        let key = version.key;
        let mut head = self.map.get(key as usize).unwrap().lock().unwrap();
        assert!(Arc::ptr_eq(&version, &*head));
        let new_head = head.previous.lock().unwrap().clone().unwrap();
        *head = new_head;
    }
}
