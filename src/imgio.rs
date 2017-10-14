use Image;
use png;
use std::error::Error;
use png::HasParameters;

pub fn load(path: &str) -> Result<Image, Box<Error>> {
    let mut decoder = png::Decoder::new(::std::fs::File::open(path)?);
    decoder.set(png::TRANSFORM_EXPAND_16);
    let (info, mut reader) = decoder.read_info()?;
    match reader.output_color_type() {
        (png::ColorType::Grayscale, png::BitDepth::Sixteen) => {
            let size = info.width as usize * info.height as usize;
            let mut arr = vec![0u8; size * 2];
            reader.next_frame(&mut arr)?;
            let mut result = vec![0f32; size];
            for i in 0..size {
                // big-endian
                let int = ((arr[i * 2] as u16) << 8) | (arr[i * 2 + 1] as u16);
                result[i] = int as f32 / (256.0 * 256.0);
            }
            Ok(Image::new(result, info.width, info.height))
        },
        (ty, depth) => {
            Err(format!("Unknown color type {:?} bit depth {:?}", ty, depth).into())
        }
    }
}

pub fn save(path: &str, image: &Image) -> Result<(), Box<Error>> {
    let size = image.size.0 as usize * image.size.1 as usize;
    let mut data = vec![0u8; size * 2];
    for i in 0..size {
        let mul = (image.data[i] * (256.0 * 256.0)).min(u16::max_value() as f32);
        let int = mul as u16;
        let high = (int >> 8) as u8;
        let low = (int & 255u16) as u8;
        // big-endian
        data[i * 2 + 0] = high;
        data[i * 2 + 1] = low;
    }
    let file = ::std::fs::File::create(path)?;
    let ref mut w = ::std::io::BufWriter::new(file);
    let mut encoder = png::Encoder::new(w, image.size.0, image.size.1);
    encoder.set(png::ColorType::Grayscale).set(png::BitDepth::Sixteen);
    let mut writer = encoder.write_header()?;
    writer.write_image_data(&data)?;
    Ok(())
}