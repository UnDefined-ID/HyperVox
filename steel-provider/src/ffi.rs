//! C interface so the game can link this crate as a static library instead of talking to the helper over a socket.
//!   hv_steel_generate(seed, chunk_x, chunk_z, dimension, &mut len) -> pointer to `len` bytes of chunk sections (same bytes the socket protocol sends after the status word),
//!                                                                    or null when generation failed / panicked
//!   hv_steel_free(ptr, len)   releases a buffer returned above
//!   hv_steel_shutdown()       drops every cached world generator
use std::collections::HashMap;
use std::sync::{Arc, Mutex, Once, OnceLock};

use crate::{Dimension, WorldgenContext, initialize, serialize_chunk_sections};

static INIT: Once = Once::new();
type Contexts = Mutex<HashMap<(u64, Dimension), Arc<WorldgenContext>>>;
static CONTEXTS: OnceLock<Contexts> = OnceLock::new();

fn contexts() -> &'static Contexts {
    CONTEXTS.get_or_init(|| Mutex::new(HashMap::new()))
}

/// # Safety
/// `out_len` must be a valid pointer to a `usize`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn hv_steel_generate(
    seed: u64,
    chunk_x: i32,
    chunk_z: i32,
    dimension: u8,
    out_len: *mut usize,
) -> *mut u8 {
    let Some(dimension) = Dimension::from_byte(dimension) else {
        return std::ptr::null_mut();
    };
    INIT.call_once(initialize);
    // one generator per (seed, dimension); creating it holds the lock so concurrent first requests build a single one
    let ctx = {
        let mut map = contexts().lock().unwrap();
        Arc::clone(
            map.entry((seed, dimension))
                .or_insert_with(|| Arc::new(WorldgenContext::new(seed, dimension))),
        )
    };
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let chunk = ctx.generate_with_structures(chunk_x, chunk_z);
        serialize_chunk_sections(&chunk)
    }));
    match result {
        Ok(bytes) => {
            let mut boxed = bytes.into_boxed_slice();
            unsafe { *out_len = boxed.len() };
            let ptr = boxed.as_mut_ptr();
            std::mem::forget(boxed);
            ptr
        }
        Err(_) => std::ptr::null_mut(),
    }
}

/// # Safety
/// `ptr`/`len` must come from a single call to [`hv_steel_generate`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn hv_steel_free(ptr: *mut u8, len: usize) {
    if !ptr.is_null() {
        drop(unsafe { Box::from_raw(std::slice::from_raw_parts_mut(ptr, len)) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn hv_steel_shutdown() {
    if let Some(c) = CONTEXTS.get() {
        c.lock().unwrap().clear();
    }
}
