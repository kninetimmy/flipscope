//! `flipscope-proto`: wire protocol types and (de)serialization for the
//! FlipScope host<->device link.
//!
//! This crate will hold the shared frame definitions, the line-oriented
//! `Key=value` wire format (PRD §26), and the protocol version constant
//! exchanged during the HELLO handshake. It has no I/O of its own — framing
//! and transport live in `flipscope-link`.

/// Wire protocol version exchanged during the HELLO handshake (PRD §26).
///
/// Placeholder value; the real constant will track the frozen wire
/// protocol once it is defined.
pub const PROTOCOL_VERSION: u32 = 0;

/// Returns the current wire protocol version.
pub fn protocol_version() -> u32 {
    PROTOCOL_VERSION
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn protocol_version_matches_constant() {
        assert_eq!(protocol_version(), PROTOCOL_VERSION);
    }
}
