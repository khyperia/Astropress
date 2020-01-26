extern crate png;

mod alg;
mod image;
mod registration;
mod stack;
mod starfinder;
mod stretch;

use crate::image::Image;
use failure::Error;
use rayon::prelude::*;
use std::{
    env::args,
    path::{Path, PathBuf},
};

fn paths_to_read(images: &mut Vec<PathBuf>, path: impl AsRef<Path>) -> Result<(), Error> {
    let path = path.as_ref();
    if path.is_dir() {
        for entry in path.read_dir()? {
            let entry = entry?;
            paths_to_read(images, entry.path())?;
        }
    } else {
        images.push(path.to_owned());
    }
    Ok(())
}

fn load_args() -> Result<Vec<Image<f64>>, Error> {
    let mut image_paths = Vec::new();
    for arg in args().skip(1) {
        paths_to_read(&mut image_paths, arg)?;
    }
    image_paths
        .par_iter()
        .map(|path| {
            println!("Reading {}", path.display());
            match Image::load(&path) {
                Ok(img) => Ok(img),
                Err(err) => Err(failure::err_msg(format!(
                    "Error loading {}: {}",
                    path.display(),
                    err,
                ))),
            }
        })
        .collect::<Result<Vec<_>, Error>>()
}

fn go() -> Result<(), Error> {
    println!("Running with {} threads", rayon::current_num_threads());
    let images = load_args()?;
    let stars = images
        .par_iter()
        .map(|img| starfinder::find_stars(img))
        .collect::<Vec<_>>();
    // TODO: if (debug_output_found_stars)
    images
        .par_iter()
        .zip(stars.par_iter())
        .enumerate()
        .map(|(i, (img, stars))| {
            let stretched = stretch::sort_stretch(img);
            let marked = starfinder::debug_mark(&stretched, stars);
            let path = format!("{}.png", i);
            println!("Saving {}", path);
            marked.save(&path)
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

fn main() -> Result<(), Error> {
    go()
}
