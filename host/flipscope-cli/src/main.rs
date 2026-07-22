//! `flipscope-cli`: headless batch entry point over `flipscope-format` and
//! `flipscope-dsp` (PRD §25). Produces offline analysis reports from
//! captured `.sub`/`.fscope` pairs without a live device connection.

/// Returns the startup banner printed before the (future) batch pipeline
/// runs.
fn banner() -> &'static str {
    "flipscope-cli: placeholder batch analysis entry point"
}

fn main() {
    println!("{}", banner());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn banner_names_the_crate() {
        assert!(banner().contains("flipscope-cli"));
    }
}
