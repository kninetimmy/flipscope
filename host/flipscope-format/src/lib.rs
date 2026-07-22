//! `flipscope-format`: `.sub` and `.fscope` read/write.
//!
//! This crate owns the RAW timing model shared by Flipper `.sub` captures
//! and the FlipScope `.fscope` sidecar (PRD §7, §25). Original captured
//! timings are immutable; derived/cleaned/analyzed data is written
//! alongside, never over them (PRD §4.5, FR-029). File I/O only — no
//! serial, no rendering.

/// Returns true if `line` is a well-formed Flipper File Format header line,
/// i.e. `Key: value` with a single colon separator followed by a space.
/// Both `.sub` and `.fscope` files are built from lines in this shape.
pub fn is_file_format_line(line: &str) -> bool {
    match line.split_once(':') {
        Some((key, value)) => !key.is_empty() && value.starts_with(' '),
        None => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn recognizes_filetype_header() {
        assert!(is_file_format_line("Filetype: FlipScope Capture Metadata"));
        assert!(!is_file_format_line("not a header line"));
    }
}
