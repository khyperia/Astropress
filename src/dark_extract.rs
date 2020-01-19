use crate::{
    alg,
    image::{AlignedImage, Image},
};

pub fn go(data: Vec<AlignedImage>) -> Image<f64> {
    let size = data[0].image.size;
    for datum in &data {
        assert_eq!(size, datum.image.size);
    }
    let mut result = Image::zero(size);
    for y in 0..size.1 {
        for x in 0..size.0 {
            let mut sum_my_offset = 0.0;
            for image_idx in 0..data.len() {
                let my_value = data[image_idx].image[(x, y)];
                let location_global = data[image_idx].local_to_global((x as f64, y as f64));
                let mut other_values = Vec::new();
                for other_image_idx in 0..data.len() {
                    if other_image_idx != image_idx {
                        let value = data[other_image_idx].get_global(location_global);
                        other_values.push(value);
                    }
                }
                let median_of_others = alg::median(&mut other_values);
                let my_offset = my_value - median_of_others;
                sum_my_offset += my_offset;
            }
            result[(x, y)] = sum_my_offset / data.len() as f64;
        }
    }
    result
}
