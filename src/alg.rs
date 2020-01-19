use crate::{image, image::Image};
use nalgebra::{self, *};

pub fn median(seq: &mut [f64]) -> f64 {
    seq.sort_unstable_by(|l, r| l.partial_cmp(&r).unwrap());
    let idx = seq.len() / 2;
    if seq.len() % 2 == 0 {
        (seq[idx - 1] + seq[idx]) / 2.0
    } else {
        seq[idx]
    }
}

pub fn mean(seq: impl IntoIterator<Item = f64>) -> f64 {
    let seq = seq.into_iter();
    let mut sum = 0.0;
    let mut count = 0;
    for item in seq {
        sum += item;
        count += 1;
    }
    sum / count as f64
}

pub fn stdev(mean: f64, seq: impl IntoIterator<Item = f64>) -> f64 {
    let seq = seq.into_iter();
    let mut sum = 0.0;
    let mut count = 0;
    for item in seq {
        let diff = mean - item;
        sum += diff * diff;
        count += 1;
    }
    (sum / count as f64).sqrt()
}

// Welford's online algorithm
pub fn mean_stdev(seq: impl IntoIterator<Item = f64>) -> (f64, f64) {
    let seq = seq.into_iter();
    let mut count = 0;
    let mut mean = 0.0;
    let mut M2 = 0.0;
    for item in seq {
        count += 1;
        let delta = item - mean;
        mean += delta / count as f64;
        let delta2 = item - mean;
        M2 += delta * delta2;
    }
    let variance = M2 / count as f64;
    (mean, variance.sqrt())
}

// least-squares solve for x:
// matrix * x = vector
// matrix is row-major (it's easier to define that way in code)
pub fn least_squares_fit(matrix: Vec<f64>, vector: Vec<f64>) -> Vec<f64> {
    assert_eq!(matrix.len() % vector.len(), 0);
    let arr_transpose: DMatrix<f64> =
        nalgebra::DMatrix::from_vec(matrix.len() / vector.len(), vector.len(), matrix);
    let arr = arr_transpose.transpose();
    let vec: DVector<f64> = nalgebra::DVector::from_vec(vector);
    // pseudo-inverse:
    // A * x = b
    // A' = (Aᵀ * A)⁻¹ * Aᵀ
    // x = A' * b
    let fucky = (&arr_transpose * arr).try_inverse().unwrap() * arr_transpose;
    let x = fucky * vec;
    x.iter().cloned().collect()
}

/*
// x, y, value
pub fn gaussian_center(data: impl Iterator<Item = (f64, f64, f64)>) -> (f64, f64) {
    let mut rows = Vec::new();
    let mut result = Vec::new();
    for (x, y, value) in data {
        rows.push(x);
        rows.push(y);
        rows.push(-(x * x + y * y));
        rows.push(1.0);
        result.push(value.ln());
    }
    let x = least_squares_fit(rows, result);
    let val_2ac = x[0];
    let val_2ad = x[1];
    let val_a = x[2];
    let c = val_2ac / (2.0 * val_a);
    let d = val_2ad / (2.0 * val_a);
    (c, d)
}
*/

// x, y, value
pub fn parabola_center(data: impl Iterator<Item = (f64, f64, f64)>) -> (f64, f64) {
    let mut rows = Vec::new();
    let mut result = Vec::new();
    for (x, y, value) in data {
        rows.push(x);
        rows.push(y);
        rows.push(-(x * x + y * y));
        rows.push(1.0);
        result.push(value);
    }
    let x = least_squares_fit(rows, result);
    let val_2ac = x[0];
    let val_2ad = x[1];
    let val_a = x[2];
    let c = val_2ac / (2.0 * val_a);
    let d = val_2ad / (2.0 * val_a);
    (c, d)
}

pub fn solve_offsets_1d(
    individual_offsets: impl Iterator<Item = (usize, usize, f64)>,
    count: usize,
) -> Vec<f64> {
    let mut matrix = Vec::new();
    let mut vector = Vec::new();
    // make nonsingular by fixing first offset=0
    for i in 0..count {
        matrix.push(if i == 0 { 1.0 } else { 0.0 });
    }
    vector.push(0.0);
    for (one, two, value) in individual_offsets {
        for i in 0..count {
            matrix.push(if i == one {
                1.0
            } else if i == two {
                -1.0
            } else {
                0.0
            });
        }
        vector.push(value);
    }
    least_squares_fit(matrix, vector)
}

fn floodfind_one<T: Copy>(
    img: &Image<T>,
    condition: &impl Fn(T) -> bool,
    coord: (usize, usize),
    mask: &mut Image<bool>,
) -> Vec<(usize, usize)> {
    let mut set = Vec::new();
    let mut next = Vec::new();
    mask[coord] = true;
    next.push(coord);
    set.push(coord);
    while let Some(coord) = next.pop() {
        let mut go = |dx, dy| {
            if let Some(coordnext) = image::offset(coord, (dx, dy), img.size) {
                if mask[coordnext] || !condition(img[coordnext]) {
                    return;
                }
                mask[coordnext] = true;
                next.push(coordnext);
                set.push(coordnext);
            }
        };
        go(-1, 0);
        go(1, 0);
        go(0, -1);
        go(0, 1);
    }
    set
}

pub fn floodfind<T: Copy>(
    img: &Image<T>,
    condition: impl Fn(T) -> bool,
) -> Vec<Vec<(usize, usize)>> {
    let mut mask = Image::zero(img.size);
    let mut results = Vec::new();
    for coord in img.iter_index() {
        if mask[coord] {
            continue;
        }
        if condition(img[coord]) {
            let result = floodfind_one(img, &condition, coord, &mut mask);
            results.push(result);
        }
    }
    results
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::f64;

    /*
    #[test]
    fn test_things() {
        let data = [
            (-1.0, -1.0, 1.0),
            (-1.0, 0.0, 2.0),
            (-1.0, 1.0, 1.0),
            (0.0, -1.0, 2.0),
            (0.0, 0.0, 3.0),
            (0.0, 1.0, 2.0),
            (1.0, -1.0, 1.0),
            (1.0, 0.0, 2.0),
            (1.0, 1.0, 1.0),
        ];
        let center = gaussian_center(data.iter().cloned());
        assert_eq!(center, (0.0, 0.0));
    }

    #[test]
    fn test_random() {
        for _ in 0..100 {
            let amplitude = rand::random::<f64>() * 100.0;
            let size = rand::random::<f64>() * 5.0 + 1.0;
            let off_x = rand::random::<f64>() * 2.0 - 1.0;
            let off_y = rand::random::<f64>() * 2.0 - 1.0;
            let mut samples = Vec::new();
            for _ in 0..100 {
                let x = rand::random::<f64>() * 2.0 - 1.0;
                let y = rand::random::<f64>() * 2.0 - 1.0;
                let numerator = (x - off_x) * (x - off_x) + (y - off_y) * (y - off_y);
                let denominator = 2.0 * size * size;
                let z = amplitude * (-numerator / denominator).exp();
                samples.push((x, y, z));
            }
            let center = gaussian_center(samples.drain(..));
            let eps = 1E-10;
            assert!((center.0 - off_x).abs() < eps, "{} != {}", center.0, off_x);
            assert!((center.1 - off_y).abs() < eps, "{} != {}", center.1, off_y);
        }
    }
    */

    macro_rules! assert_delta {
        ($x:expr, $y:expr, $d:expr) => {
            if ($x - $y).abs() > $d {
                panic!(
                    "{} == {} - off by {}x epsilon",
                    $x,
                    $y,
                    ($x - $y).abs() / $d
                );
            }
        };
    }

    #[test]
    fn test_mean_stdev() {
        for _ in 0..100 {
            let x = (0..100).map(|_| rand::random::<f64>()).collect::<Vec<_>>();
            let mean = mean(x.iter().cloned());
            let stdev = stdev(mean, x.iter().cloned());
            let (mean2, stdev2) = mean_stdev(x.iter().cloned());
            assert_delta!(mean, mean2, f64::EPSILON * 4.0);
            assert_delta!(stdev, stdev2, f64::EPSILON * 4.0);
        }
    }
}
