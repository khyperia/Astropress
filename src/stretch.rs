use crate::image::Image;

pub fn sort_stretch(img: &Image<f64>) -> Image<f64> {
    let mut data = img
        .iter_index()
        .map(|coord| (img[coord], coord))
        .collect::<Vec<_>>();
    data.sort_unstable_by(|(l, _), (r, _)| l.partial_cmp(r).unwrap());
    let mut result = Image::zero(img.size);
    let len = data.len() as f64;
    for (i, (_, coord)) in data.into_iter().enumerate() {
        result[coord] = i as f64 / len;
    }
    result
}
