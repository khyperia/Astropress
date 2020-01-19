extern crate png;

mod alg;
mod dark_extract;
mod image;
mod imgio;
mod registration;
mod stack;
mod starfinder;
mod stretch;

use failure::Error;
use rayon::prelude::*;
use serde::Deserialize;
use std::{
    env::args,
    path::{Path, PathBuf},
};

fn read(images: &mut Vec<PathBuf>, path: impl AsRef<Path>) -> Result<(), Error> {
    let path = path.as_ref();
    if path.is_dir() {
        for entry in path.read_dir()? {
            let entry = entry?;
            read(images, entry.path())?;
        }
    } else {
        images.push(path.to_owned());
    }
    Ok(())
}

fn go() -> Result<(), Error> {
    println!("Running with {} threads", rayon::current_num_threads());
    let mut image_paths = Vec::new();
    for arg in args().skip(1) {
        read(&mut image_paths, arg)?;
    }
    let images = image_paths
        .par_iter()
        .map(|path| {
            println!("Reading {}", path.display());
            match imgio::load(&path) {
                Ok(img) => Ok(img),
                Err(err) => Err(failure::err_msg(format!(
                    "Error loading {}: {}",
                    path.display(),
                    err,
                ))),
            }
        })
        .collect::<Result<Vec<_>, Error>>()?;
    let stars = images
        .par_iter()
        .map(|img| starfinder::find_stars(img))
        .collect::<Vec<_>>();
    images
        .par_iter()
        .zip(stars.par_iter())
        .enumerate()
        .map(|(i, (img, stars))| {
            let stretched = stretch::sort_stretch(img);
            let marked = starfinder::debug_mark(&stretched, stars);
            let path = format!("{}.png", i);
            println!("Saving {}", path);
            imgio::save_rgb(&path, &marked)
        })
        .collect::<Result<(), Error>>()?;
    /*
    let aligned = registration::register(&images);
    for (x, y) in aligned {
        println!("{} {}", x, y);
    }
    */
    // let stacked = stack::stack_max(&images);
    // let hot = stack::hot_detect(&stacked);
    // for (i, image) in images.iter_mut().enumerate() {
    //     stack::hot_filter(image, &hot);
    //     println!("Saving {}.png", i);
    //     imgio::save(&format!("{}.png", i), image)?;
    // }
    // println!("Saving hot.png");
    // imgio::save("hot.png", &hot)?;
    Ok(())
}

#[allow(non_snake_case)]
#[derive(Debug, Deserialize)]
struct Record {
    Path: PathBuf,
    File: PathBuf,
    dX: f64,
    dY: f64,
    Angle: String,
}

fn do_dark_extract() -> Result<(), Error> {
    let file = args()
        .nth(1)
        .ok_or_else(|| failure::err_msg("Error: must provide DSS info file"))?;
    let mut reader = csv::ReaderBuilder::new().delimiter(b'\t').from_path(file)?;
    let mut images = Vec::new();
    for record in reader.deserialize() {
        let mut record: Record = record?;
        record.Path.push(record.File);
        record.Path.set_extension("png");
        let image = image::AlignedImage {
            image: imgio::load(record.Path)?,
            d_x: record.dX,
            d_y: record.dY,
            angle: record.Angle.replace(" °", "").parse()?,
        };
        images.push(image);
    }
    let result = dark_extract::go(images);
    imgio::save("fuck.png", &result)?;
    Ok(())
}

fn main() -> Result<(), Error> {
    go()
    //do_dark_extract()
}
