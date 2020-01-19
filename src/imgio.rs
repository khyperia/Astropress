use crate::image::Image;
use failure::Error;
use png;
use std::{fs::File, path::Path};

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

fn f64_to_u16(mut value: f64) -> u16 {
    let max_value = f64::from(u16::max_value());
    value *= max_value;
    if value >= max_value {
        u16::max_value()
    } else if value > 0.0 {
        value as u16
    } else {
        0
    }
}

pub fn f64_to_u8(mut value: f64) -> u8 {
    let max_value = f64::from(u8::max_value());
    value *= max_value;
    if value >= max_value {
        u8::max_value()
    } else if value > 0.0 {
        value as u8
    } else {
        0
    }
}

pub fn save(path: impl AsRef<Path>, img: &Image<f64>) -> Result<(), Error> {
    let max = img
        .data
        .iter()
        .cloned()
        .max_by(|l, r| l.partial_cmp(r).unwrap())
        .unwrap();
    let min = img
        .data
        .iter()
        .cloned()
        .min_by(|l, r| l.partial_cmp(r).unwrap())
        .unwrap();
    let mut encoder = png::Encoder::new(File::create(path)?, img.size.0 as u32, img.size.1 as u32);
    encoder.set_color(png::ColorType::Grayscale);
    encoder.set_depth(png::BitDepth::Sixteen);
    let mut writer = encoder.write_header()?;
    let mut output = vec![0; img.size.0 * img.size.1 * 2];
    for i in 0..(img.size.0 * img.size.1) {
        let value_float = (img.data[i] - min) / (max - min);
        let value = f64_to_u16(value_float);
        output[i * 2] = (value >> 8) as u8;
        output[i * 2 + 1] = (value) as u8;
    }
    writer.write_image_data(&output)?;
    Ok(())
}

pub fn save_rgb(path: impl AsRef<Path>, img: &Image<[u8; 3]>) -> Result<(), Error> {
    let mut encoder = png::Encoder::new(File::create(path)?, img.size.0 as u32, img.size.1 as u32);
    encoder.set_color(png::ColorType::RGB);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header()?;
    let mut output = vec![0; img.size.0 * img.size.1 * 3];
    for i in 0..(img.size.0 * img.size.1) {
        for c in 0..3 {
            output[i * 3 + c] = img.data[i][c];
        }
    }
    writer.write_image_data(&output)?;
    Ok(())
}
