//! `flipscope-dsp`: the Stage 3 signal-processing engine.
//!
//! Pure functions over the pulse-timing and RSSI-sweep data captured by
//! Stage 1 (PRD §25, §28). No I/O, no threads, and — deliberately — no
//! external dependencies: every stage is implemented from first
//! principles and is developed and tested against synthetic and recorded
//! pulse streams with zero hardware in the loop.

/// Computes the arithmetic mean of a slice of pulse-timing samples
/// (microseconds). Returns `None` for an empty slice.
///
/// This is a placeholder for the real analysis stages (histogram, Te
/// estimation, autocorrelation, ...) that land in later H-milestones.
pub fn mean_duration_us(samples: &[u32]) -> Option<f64> {
    if samples.is_empty() {
        return None;
    }
    let sum: u64 = samples.iter().map(|&s| u64::from(s)).sum();
    Some(sum as f64 / samples.len() as f64)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mean_of_three_samples() {
        assert_eq!(mean_duration_us(&[100, 200, 300]), Some(200.0));
    }

    #[test]
    fn mean_of_empty_is_none() {
        assert_eq!(mean_duration_us(&[]), None);
    }
}
