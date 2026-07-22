//! `flipscope-link`: USB CDC serial transport to the Flipper Zero device.
//!
//! Owns framing, reconnect, and backpressure over the wire protocol
//! defined in `flipscope-proto` (PRD §25, §26). This is the only crate
//! where serial I/O happens; no transport code lives elsewhere.

/// Returns true if `buf` contains at least one complete line-oriented
/// frame, i.e. a `\n`-terminated line, per the wire protocol framing rule
/// (PRD §26).
pub fn has_complete_frame(buf: &[u8]) -> bool {
    buf.contains(&b'\n')
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn detects_terminated_frame() {
        assert!(has_complete_frame(b"HELLO proto=1\n"));
        assert!(!has_complete_frame(b"HELLO proto=1"));
    }
}
