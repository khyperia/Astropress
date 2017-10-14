extern crate png;

mod imgio;
mod stack;

use std::env::args;
use std::error::Error;

#[derive(Clone)]
pub struct Image {
    data: Vec<f32>,
    size: (u32, u32),
}

impl Image {
    fn new(data: Vec<f32>, width: u32, height: u32) -> Image {
        Image {
            data: data,
            size: (width, height),
        }
    }

    fn zero(size: (u32, u32)) -> Image {
        Image {
            data: vec![0f32; size.0 as usize * size.1 as usize],
            size: size,
        }
    }

    fn mid_left_right_up_down(&self, pos: (u32, u32)) -> (f32, f32, f32, f32, f32) {
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
        let up = if pos.1 == 0{
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
}

impl std::ops::Index<(u32, u32)> for Image {
    type Output = f32;
    fn index(&self, index: (u32, u32)) -> &f32 {
        if index.0 >= self.size.0 || index.1 >= self.size.1 {
            panic!("Index out of range: {:?} (size {:?})", index, self.size)
        }
        self.data.index(self.size.0 as usize * index.1 as usize + index.0 as usize)
    }
}

impl std::ops::IndexMut<(u32, u32)> for Image {
    fn index_mut(&mut self, index: (u32, u32)) -> &mut f32 {
        if index.0 >= self.size.0 || index.1 >= self.size.1 {
            panic!("Index out of range: {:?} (size {:?})", index, self.size)
        }
        self.data.index_mut(self.size.0 as usize * index.1 as usize + index.0 as usize)
    }
}

fn go() -> Result<(), Box<Error>> {
    let mut images = Vec::new();
    for arg in args().skip(1) {
        println!("Reading {}", arg);
        match imgio::load(&arg) {
            Ok(img) => images.push(img),
            Err(err) => return Err(format!("Error loading {}: {}", arg, err).into()),
        }
    }
    let stacked = stack::stack_max(&images);
    let hot = stack::hot_detect(&stacked);
    let mut i = 0;
    for image in &mut images {
        stack::hot_filter(image, &hot);
        println!("Saving {}.png", i);
        imgio::save(&format!("{}.png", i), image)?;
        i += 1;
    }
    println!("Saving hot.png");
    imgio::save("hot.png", &hot)?;
    Ok(())
}

fn main() {
    match go() {
        Ok(()) => (),
        Err(err) => println!("Error: {}", err)
    }
}
