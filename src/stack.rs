use crate::image::Image;

fn stack<F: Fn(f64, f64) -> f64>(images: &[Image<f64>], f: F) -> Image<f64> {
    let mut result = images[0].clone();
    for image in images.iter().skip(1) {
        if image.size != result.size {
            panic!(
                "Mismatched sizes during stacking: {:?} and {:?}",
                image.size, result.size
            );
        }
        for (left, right) in result.data.iter_mut().zip(image.data.iter()) {
            *left = f(*left, *right);
        }
    }
    result
}

pub fn mean_stdev(values: &[f64]) -> (f64, f64) {
    let mean = values.iter().sum::<f64>() / values.len() as f64;
    let ssd = values.iter().map(|x| x - mean).map(|x| x * x).sum::<f64>();
    let variance = ssd / (values.len() as f64);
    let stdev = variance.sqrt();
    (mean, stdev)
}

pub fn stack_max(images: &[Image<f64>]) -> Image<f64> {
    stack(images, |l, r| l.max(r))
}

pub fn hot_detect(image: &Image<f64>) -> Image<f64> {
    let mut result = Image::zero(image.size);
    let mut num = 0usize;
    for y in 0..image.size.1 {
        for x in 0..image.size.0 {
            let (mid, left, right, up, down) = image.mid_left_right_up_down((x, y));
            let (mean, stdev) = mean_stdev(&[left, right, up, down]);
            if mid > mean + stdev * 2.0 {
                result[(x, y)] = 1.0;
                num += 1;
            }
        }
    }
    println!(
        "{}% ({}) hot pixels detected",
        (num as f64 * 100.0) / (image.size.0 as usize * image.size.1 as usize) as f64,
        num
    );
    result
}

pub fn hot_filter(image: &Image<f64>, filter: &Image<f64>) -> Image<f64> {
    let mut result = image.clone();
    for (i, (result_pix, filter_pix)) in result.data.iter_mut().zip(filter.data.iter()).enumerate()
    {
        if *filter_pix > 0.0 {
            let x = i % image.size.0 as usize;
            let y = i / image.size.0 as usize;
            let (_, left, right, up, down) = image.mid_left_right_up_down((x, y));
            let avg = (left + right + up + down) * 0.25;
            *result_pix = avg;
        }
    }
    result
}
