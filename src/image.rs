use crate::alg::f64_to_u16;
use failure::Error;
use rustfft::{num_complex::Complex64, num_traits::identities::Zero, FFT};
use std::{cell::RefCell, collections::HashMap, fs::File, path::Path, sync::Arc};

#[derive(Clone)]
pub struct Image<T: Copy> {
    pub data: Vec<T>,
    pub size: (usize, usize),
}

impl<T: Copy + Default> Image<T> {
    pub fn zero(size: (usize, usize)) -> Image<T> {
        Self::new_val(T::default(), size)
    }
}

impl<T: Copy> Image<T> {
    pub fn new(data: Vec<T>, size: (usize, usize)) -> Self {
        assert_eq!(data.len(), size.0 * size.1);
        Self { data, size }
    }

    pub fn new_val(data: T, size: (usize, usize)) -> Self {
        Self::new(vec![data; size.0 * size.1], size)
    }

    pub fn mid_left_right_up_down(&self, pos: (usize, usize)) -> (T, T, T, T, T) {
        let mid = self[pos];
        let left = if pos.0 == 0 {
            mid
        } else {
            self[(pos.0 - 1, pos.1)]
        };
        let right = if pos.0 + 1 == self.size.0 {
            mid
        } else {
            self[(pos.0 + 1, pos.1)]
        };
        let up = if pos.1 == 0 {
            mid
        } else {
            self[(pos.0, pos.1 - 1)]
        };
        let down = if pos.1 + 1 == self.size.1 {
            mid
        } else {
            self[(pos.0, pos.1 + 1)]
        };
        (mid, left, right, up, down)
    }

    pub fn get_clamped(&self, mut x: isize, mut y: isize) -> T {
        if x < 0 {
            x = 0;
        } else if x >= self.size.0 as isize {
            x = self.size.0 as isize - 1;
        }
        if y < 0 {
            y = 0;
        } else if y >= self.size.1 as isize {
            y = self.size.1 as isize - 1;
        }
        self[(x as usize, y as usize)]
    }

    pub fn get_wrapped(&self, mut x: isize, mut y: isize) -> T {
        x = x.rem_euclid(self.size.0 as isize);
        y = y.rem_euclid(self.size.1 as isize);
        self[(x as usize, y as usize)]
    }

    pub fn iter_index(&self) -> impl Iterator<Item = (usize, usize)> {
        Range2d::new(self.size)
    }
}

pub fn offset(
    coord: (usize, usize),
    delta: (isize, isize),
    size: (usize, usize),
) -> Option<(usize, usize)> {
    use std::convert::TryInto;
    let coordnext = match (
        (coord.0 as isize + delta.0).try_into().ok(),
        (coord.1 as isize + delta.1).try_into().ok(),
    ) {
        (Some(x), Some(y)) => (x, y),
        _ => return None,
    };
    if coordnext.0 >= size.0 || coordnext.1 >= size.1 {
        None
    } else {
        Some(coordnext)
    }
}

impl<T: Copy> std::ops::Index<(usize, usize)> for Image<T> {
    type Output = T;
    fn index(&self, index: (usize, usize)) -> &T {
        if index.0 >= self.size.0 || index.1 >= self.size.1 {
            panic!("Index out of range: {:?} (size {:?})", index, self.size)
        }
        self.data
            .index(self.size.0 as usize * index.1 as usize + index.0 as usize)
    }
}

impl<T: Copy> std::ops::IndexMut<(usize, usize)> for Image<T> {
    fn index_mut(&mut self, index: (usize, usize)) -> &mut T {
        if index.0 >= self.size.0 || index.1 >= self.size.1 {
            panic!("Index out of range: {:?} (size {:?})", index, self.size)
        }
        self.data
            .index_mut(self.size.0 as usize * index.1 as usize + index.0 as usize)
    }
}

impl<'a, T: Copy> IntoIterator for &'a Image<T> {
    type Item = T;
    type IntoIter = std::iter::Cloned<std::slice::Iter<'a, T>>;
    fn into_iter(self) -> <Self as IntoIterator>::IntoIter {
        self.data.iter().cloned()
    }
}

impl Image<f64> {
    pub fn to_complex(&self) -> Image<Complex64> {
        Image::new(
            self.data.iter().map(|&x| (x as f64).into()).collect(),
            self.size,
        )
    }

    pub fn save(&self, path: impl AsRef<Path>) -> Result<(), Error> {
        let max = self
            .data
            .iter()
            .cloned()
            .max_by(|l, r| l.partial_cmp(r).unwrap())
            .unwrap();
        let min = self
            .data
            .iter()
            .cloned()
            .min_by(|l, r| l.partial_cmp(r).unwrap())
            .unwrap();
        let mut encoder =
            png::Encoder::new(File::create(path)?, self.size.0 as u32, self.size.1 as u32);
        encoder.set_color(png::ColorType::Grayscale);
        encoder.set_depth(png::BitDepth::Sixteen);
        let mut writer = encoder.write_header()?;
        let mut output = vec![0; self.size.0 * self.size.1 * 2];
        for i in 0..(self.size.0 * self.size.1) {
            let value_float = (self.data[i] - min) / (max - min);
            let value = f64_to_u16(value_float);
            output[i * 2] = (value >> 8) as u8;
            output[i * 2 + 1] = (value) as u8;
        }
        writer.write_image_data(&output)?;
        Ok(())
    }

    pub fn load(path: impl AsRef<Path>) -> Result<Image<f64>, Error> {
        let mut decoder = png::Decoder::new(File::open(path)?);
        decoder.set_transformations(png::Transformations::IDENTITY);
        let (info, mut reader) = decoder.read_info()?;
        assert_eq!(info.bit_depth, png::BitDepth::Sixteen);
        assert_eq!(info.color_type, png::ColorType::Grayscale);
        let mut buf = vec![0; info.buffer_size()];
        reader.next_frame(&mut buf)?;
        let mut buff64 = vec![0.0; info.width as usize * info.height as usize];
        for i in 0..buff64.len() {
            let short_val = u16::from(buf[i * 2]) << 8 | u16::from(buf[i * 2 + 1]);
            buff64[i] = f64::from(short_val) / f64::from(u16::max_value());
        }
        Ok(Image::new(
            buff64,
            (info.width as usize, info.height as usize),
        ))
    }
}

impl Image<[u8; 3]> {
    pub fn save(&self, path: impl AsRef<Path>) -> Result<(), Error> {
        let mut encoder =
            png::Encoder::new(File::create(path)?, self.size.0 as u32, self.size.1 as u32);
        encoder.set_color(png::ColorType::RGB);
        encoder.set_depth(png::BitDepth::Eight);
        let mut writer = encoder.write_header()?;
        let mut output = vec![0; self.size.0 * self.size.1 * 3];
        for i in 0..(self.size.0 * self.size.1) {
            for c in 0..3 {
                output[i * 3 + c] = self.data[i][c];
            }
        }
        writer.write_image_data(&output)?;
        Ok(())
    }
}

impl Image<Complex64> {
    pub fn to_real(&self) -> Image<f64> {
        Image::new(self.data.iter().map(|&x| x.re).collect(), self.size)
    }

    pub fn weird_mul(&self, other: &Self) -> Self {
        let out = self
            .data
            .iter()
            .zip(other.data.iter())
            .map(|(a, b)| {
                let prod = a * b.conj();
                prod / prod.norm()
            })
            .collect::<Vec<_>>();
        Self::new(out, self.size)
    }

    fn transpose(&self) -> Self {
        let mut output = Self::new(
            vec![Complex64::zero(); self.data.len()],
            (self.size.1, self.size.0),
        );
        for y in 0..self.size.1 {
            for x in 0..self.size.0 {
                output[(y, x)] = self[(x, y)];
            }
        }
        output
    }

    // must be in-place because process_multi wrecks the input
    fn fft_x(&mut self, inverse: bool) {
        type FFT64 = Arc<dyn FFT<f64>>;
        thread_local! {
            static PLAN_MAP: RefCell<HashMap<(usize, bool), FFT64>> = RefCell::new(HashMap::new());
        }
        PLAN_MAP.with(|plan_map| {
            let mut plan_map = plan_map.borrow_mut();
            let f = plan_map
                .entry((self.size.0, inverse))
                .or_insert_with(|| rustfft::FFTplanner::new(inverse).plan_fft(self.size.0));
            let mut output = vec![Complex64::zero(); self.data.len()];
            f.process_multi(&mut self.data, &mut output);
            self.data = output;
        })
    }

    pub fn fft(&self, inverse: bool) -> Self {
        let mut res = self.transpose();
        res.fft_x(inverse);
        res = res.transpose();
        res.fft_x(inverse);
        res
    }

    pub fn median_filter(&self) -> Self {
        let data = self
            .iter_index()
            .map(|(x, y)| {
                let x = x as isize;
                let y = y as isize;
                let mut values = [
                    self.get_clamped(x - 1, y - 1),
                    self.get_clamped(x, y - 1),
                    self.get_clamped(x + 1, y),
                    self.get_clamped(x - 1, y),
                    self.get_clamped(x, y),
                    self.get_clamped(x + 1, y),
                    self.get_clamped(x - 1, y + 1),
                    self.get_clamped(x, y + 1),
                    self.get_clamped(x + 1, y + 1),
                ];
                // TODO: partition_at_index_by
                values.sort_unstable_by(|l, r| l.re.partial_cmp(&r.re).unwrap());
                values[4]
            })
            .collect();
        Self {
            data,
            size: self.size,
        }
    }
}

struct Range2d {
    size: (usize, usize),
    cur: (usize, usize),
}

impl Range2d {
    fn new(size: (usize, usize)) -> Self {
        Self { size, cur: (0, 0) }
    }
}

impl Iterator for Range2d {
    type Item = (usize, usize);
    fn next(&mut self) -> Option<(usize, usize)> {
        if self.cur.1 >= self.size.1 {
            None
        } else {
            let result = self.cur;
            self.cur.0 += 1;
            if self.cur.0 >= self.size.0 {
                self.cur.0 = 0;
                self.cur.1 += 1;
            }
            Some(result)
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let size = self.size.0 * self.size.1;
        (size, Some(size))
    }
}

pub struct AlignedImage {
    pub image: Image<f64>,
    pub d_x: f64,
    pub d_y: f64,
    pub angle: f64,
}

impl AlignedImage {
    pub fn local_to_global(&self, local: (f64, f64)) -> (f64, f64) {
        (local.0 + self.d_x, local.1 + self.d_y)
    }

    pub fn global_to_local(&self, global: (f64, f64)) -> (f64, f64) {
        (global.0 - self.d_x, global.1 - self.d_y)
    }

    pub fn get_global(&self, global: (f64, f64)) -> f64 {
        let (x, y) = self.global_to_local(global);
        if x < 0.0 || x >= self.image.size.0 as f64 || y < 0.0 || y >= self.image.size.1 as f64 {
            0.0
        } else {
            self.image[(x as usize, y as usize)]
        }
    }
}
