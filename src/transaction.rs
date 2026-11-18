use std::{collections::HashMap, sync::{Arc, Mutex}};

use crate::{timestamp::Timestamp, version::Version};

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TxnId(pub u64);
