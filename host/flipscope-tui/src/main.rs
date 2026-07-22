//! `flipscope-tui`: terminal remote head for the Flipper Zero device.
//!
//! Renders device state (spectrum, waterfall, peaks, activity monitor)
//! streamed over `flipscope-link`, and sends operator intent back to the
//! device, which remains authoritative (PRD §2, §25). Placeholder entry
//! point until the H2+ milestones land the real ratatui UI.

/// Returns the startup banner printed before the (future) TUI takes over
/// the terminal.
fn banner() -> &'static str {
    "flipscope-tui: placeholder terminal remote head"
}

fn main() {
    println!("{}", banner());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn banner_names_the_crate() {
        assert!(banner().contains("flipscope-tui"));
    }
}
