use crate::{alg, image::Image};
use rayon::prelude::*;
use rustfft::{num_complex::Complex64, num_traits::identities::Zero, FFT};

// pub fn register(images: &[Image<f64>]) -> Vec<(f64, f64)> {
//     register_complex(&images.iter().map(|i| i.to_complex()).collect::<Vec<_>>())
// }

fn solve_offsets(offsets: Vec<(usize, usize, (f64, f64))>, count: usize) -> Vec<(f64, f64)> {
    let x = alg::solve_offsets_1d(offsets.iter().map(|&(x, y, (v, _))| (x, y, v)), count);
    let y = alg::solve_offsets_1d(offsets.into_iter().map(|(x, y, (_, v))| (x, y, v)), count);
    x.into_iter().zip(y.into_iter()).collect()
}

pub fn register(images: &[Image<f64>]) -> Vec<(f64, f64)> {
    println!("FFT'ing images");
    let fft_images = images
        .par_iter()
        .map(|x| x.to_complex().fft(false))
        .collect::<Vec<_>>();
    println!("Done FFT'ing");
    // let mut progress = 0;
    // let progress_total = (images.len() * images.len() - images.len()) / 2;
    let iter = (0..(images.len() - 1))
        .map(|one| (one, images.len() - 1))
        .collect::<Vec<_>>();
    let offsets = iter
        .par_iter()
        .map(|&(one, two)| {
            //println!("Align {}%", 100 * progress / progress_total);
            let mul = fft_images[one].weird_mul(&fft_images[two]).fft(true);
            let median = mul.median_filter();
            let offset = get_offset(&median);
            println!("offset between image {} and {} is {:?}", one, two, offset);
            (one, two, offset)
        })
        .filter(|(_, _, (x, y))| !x.is_nan() && !y.is_nan())
        .collect();
    solve_offsets(offsets, images.len())
}

/*
pub fn register(images: &[Image<f64>]) -> Vec<(f64, f64)> {
    println!("FFT'ing images");
    let fft_images = images
        .par_iter()
        .map(|x| x.to_complex().fft(false))
        .collect::<Vec<_>>();
    println!("Done FFT'ing");
    // let mut progress = 0;
    // let progress_total = (images.len() * images.len() - images.len()) / 2;
    let iter = (0..images.len())
        .flat_map(|one| ((one + 1)..images.len()).map(move |two| (one, two)))
        .collect::<Vec<_>>();
    let offsets = iter
        .par_iter()
        .map(|&(one, two)| {
            //println!("Align {}%", 100 * progress / progress_total);
            let mul = fft_images[one].weird_mul(&fft_images[two]).fft(true);
            let median = mul.median_filter();
            let offset = get_offset(&median);
            println!("offset between image {} and {} is {:?}", one, two, offset);
            (one, two, offset)
        })
        .filter(|(_, _, (x, y))| !x.is_nan() && !y.is_nan())
        .collect();
    solve_offsets(offsets, images.len())
}
*/

fn get_offset(image: &Image<Complex64>) -> (f64, f64) {
    let uid: f64 = image.iter_index().take(100).map(|i| image[i].re).sum();
    crate::imgio::save(format!("{}.png", uid), &image.to_real()).unwrap();

    let (max_x, max_y) = image
        .iter_index()
        .map(|i| (i, image[i]))
        .max_by(|(_, l), (_, r)| l.re.partial_cmp(&r.re).unwrap())
        .unwrap()
        .0;
    let mut center = (max_x as f64, max_y as f64);
    // let mut samples = Vec::new();
    // for dy in (-1)..2 {
    //     for dx in (-1)..2 {
    //         let x = max_x as isize + dx;
    //         let y = max_y as isize + dy;
    //         let v = image.get_wrapped(x, y).re;
    //         samples.push((x as f64, y as f64, v));
    //     }
    // }
    // //let mut center = alg::gaussian_center(samples.into_iter());
    // let mut center = alg::parabola_center(samples.into_iter());
    if center.0 > image.size.0 as f64 / 2.0 {
        center.0 -= image.size.0 as f64;
    }
    if center.1 > image.size.1 as f64 / 2.0 {
        center.1 -= image.size.1 as f64;
    }
    center
}
