use crate::{
    alg::{f64_to_u8, floodfind, mean_stdev},
    image,
    image::Image,
};

/*
Alternate algorithm:

1) Find brightest pixel
2) Flood fill out to brightest neighbor, until certain number of pixels in size, or threshhold is reached (median of surroundings?)
3) Mask out star, goto 1, until N stars are found
*/

pub fn find_stars(img: &Image<f64>) -> Vec<(f64, f64)> {
    let (mean, stdev) = mean_stdev(img);
    let noise_sigma = 3.0;
    let floor = mean + stdev * noise_sigma;
    let min_size = 5;
    let max_size = 100;
    //let mut debug_img = Image::zero(img.size);
    let mut stars = Vec::new();
    for star in floodfind(img, |v| v > floor) {
        if star.len() > max_size {
            //println!("big boi: {}", star.len());
            //for pixel in star {
            //    debug_img[pixel] = [255, 0, 0];
            //}
            continue;
        }
        if star.len() < min_size {
            //for pixel in star {
            //    debug_img[pixel] = [0, 0, 255];
            //}
            continue;
        }
        //println!("star glob ok: {}", star.len());
        let mut sum = (0.0, 0.0);
        let mut count = 0.0;
        for pixel in star {
            //debug_img[pixel] = [0, 255, 0];
            let weight = img[pixel] - floor;
            sum = (
                sum.0 + pixel.0 as f64 * weight,
                sum.1 + pixel.1 as f64 * weight,
            );
            count += weight;
        }
        let average = (sum.0 / count, sum.1 / count);
        stars.push(average);
    }
    //crate::imgio::save_rgb("mask.png", &debug_img).unwrap();
    stars
}

pub fn debug_mark(img: &Image<f64>, stars: &[(f64, f64)]) -> Image<[u8; 3]> {
    println!("{} stars found", stars.len());
    let mut new_img = Image::new(
        img.data
            .iter()
            .map(|&v| {
                let v = f64_to_u8(v);
                [v, v, v]
            })
            .collect(),
        img.size,
    );
    for star in stars {
        fn go(img: &mut Image<[u8; 3]>, coord: (usize, usize), delta: (isize, isize)) {
            for index in (-5)..6 {
                let delta = (delta.0 * index, delta.1 * index);
                if let Some(coord) = image::offset(coord, delta, img.size) {
                    img[coord][0] = u8::max_value();
                    img[coord][1] = 0;
                    img[coord][2] = 0;
                }
            }
        }
        let coord = (star.0 as usize, star.1 as usize);
        go(&mut new_img, coord, (1, 0));
        go(&mut new_img, coord, (0, 1));
    }
    new_img
}
