use crate::{alg, image::Image, starfinder::Star};
use rayon::prelude::*;

fn solve_offsets(offsets: Vec<(usize, usize, (f64, f64))>, count: usize) -> Vec<(f64, f64)> {
    let x = alg::solve_offsets_1d(offsets.iter().map(|&(x, y, (v, _))| (x, y, v)), count);
    let y = alg::solve_offsets_1d(offsets.into_iter().map(|(x, y, (_, v))| (x, y, v)), count);
    x.into_iter().zip(y.into_iter()).collect()
}

fn register_one(left: &[Star], right: &[Star]) -> (f64, f64) {
    unimplemented!();
}

pub fn register(images: &[Vec<Star>]) -> Vec<(f64, f64)> {
    // let mut progress = 0;
    // let progress_total = (images.len() * images.len() - images.len()) / 2;
    let iter = (0..(images.len() - 1))
        .flat_map(|one| ((one + 1)..images.len()).map(move |two| (one, two)))
        .collect::<Vec<_>>();
    let offsets = iter
        .par_iter()
        .map(|&(one, two)| {
            let offset = register_one(&images[one], &images[two]);
            println!("offset between image {} and {} is {:?}", one, two, offset);
            (one, two, offset)
        })
        .filter(|(_, _, (x, y))| !x.is_nan() && !y.is_nan())
        .collect();
    solve_offsets(offsets, images.len())
}
