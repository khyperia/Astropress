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

pub struct Star {
    pub x: f64,
    pub y: f64,
    // flux isn't *totally* correct, it's underestimated, since the fringes of the star are cut off and not added
    pub flux: f64,
}

impl Star {
    pub fn new(x: f64, y: f64, flux: f64) -> Self {
        Self { x, y, flux }
    }
}

pub fn find_stars(img: &Image<f64>) -> Vec<Star> {
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
        let mut total_flux_above = 0.0;
        let star_len = star.len();
        for pixel in star {
            //debug_img[pixel] = [0, 255, 0];
            let flux_above = img[pixel] - floor;
            sum = (
                sum.0 + pixel.0 as f64 * flux_above,
                sum.1 + pixel.1 as f64 * flux_above,
            );
            total_flux_above += flux_above;
        }
        let average = (sum.0 / total_flux_above, sum.1 / total_flux_above);
        let total_flux = total_flux_above + star_len as f64 * (stdev * noise_sigma);
        stars.push(Star::new(average.0, average.1, total_flux));
    }
    //crate::imgio::save_rgb("mask.png", &debug_img).unwrap();
    stars
}

pub fn debug_mark(img: &Image<f64>, stars: &[Star]) -> Image<[u8; 3]> {
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
        let coord = (star.x as usize, star.y as usize);
        go(&mut new_img, coord, (1, 0));
        go(&mut new_img, coord, (0, 1));
    }
    new_img
}
