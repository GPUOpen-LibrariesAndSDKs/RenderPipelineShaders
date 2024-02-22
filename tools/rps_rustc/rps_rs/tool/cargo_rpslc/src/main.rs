use std::{env, fs, io::Write, process::Command};

#[derive(Default)]
struct Args
{
    pub src: Option<String>,
    pub out_dir: Option<String>,
    pub out_file: Option<String>,
    pub expand_macros: bool,
    pub cargo_args: Vec<String>,
}

fn parse_args() -> Args
{
    let mut out_args = Args::default();

    let mut args = env::args();

    args.nth(0);

    out_args.src = args.nth(0).map(|a| if std::path::Path::new(&a).exists() { Some(a) } else { None } ).flatten();

    while let Some(arg) = args.nth(0)
    {
        println!("arg: {}", &arg);
        match arg.as_str() {
            "--" => {
                out_args.cargo_args = args.collect();
                break;
            },
            "-o" => {
                out_args.out_file = args.nth(0);
            },
            "-od" => {
                out_args.out_dir = args.nth(0);
            },
            "-expand" => {
                out_args.expand_macros = true;
            },
            _ => {}
        }
    }

    out_args
}

fn main() {

    let args = parse_args();

    let src = match args.src {
        Some(arg0) => {
            if std::path::PathBuf::from(&arg0).exists() {
                Some(arg0)
            }
            else { None }
        },
        _ => None,
    };

    if src.is_none() {
        println!(r"
Usage: cargo_rpslc <source_file_name> [cargo_rpslc options...] [-- [cargo options...]]
cargo_rpslc options:
    -o: output file name
    -od: output directory
    -expand: expand macros (use cargo-expand subcommand)
");
        return;
    }

    let src = src.unwrap();

    let src_path = std::path::PathBuf::from(&src);
    println!("Processing src file: '{}'", src);

    let mut src_file_stem : String = src_path.file_stem().unwrap().to_str().unwrap().into();
    if let Some(minimum_stem) = src_file_stem.split_once('.') {
        src_file_stem = minimum_stem.0.into();
    }
    println!("Package name: '{}'", &src_file_stem);

    let tmp_dir = env::temp_dir();
    let tmp_dir = tmp_dir.join(r".cargo-rpslc\").join(&src_file_stem);

    println!("Temporary dir: '{}'", tmp_dir.to_str().unwrap());
    fs::create_dir_all(&tmp_dir).expect("Failed to create temp folder.");

    let current_exe_dir = env::current_exe().unwrap();
    let current_exe_dir = current_exe_dir.parent().unwrap();
    let render_graph_lib_dir = current_exe_dir.join("../../rps_rs").canonicalize().unwrap();
    let render_graph_lib_dir = render_graph_lib_dir.to_str().unwrap().replace("\\", "\\\\");
    let src_path = src_path.canonicalize().unwrap().to_str().unwrap().replace("\\", "\\\\");

    println!("Using render_graph library from: '{}'", &render_graph_lib_dir);

    let toml_text = format!(
r#"
[package]
name = "{0}"
version = "0.1.0"
edition = "2021"

[dependencies]
bitflags = "2.4.1"
linkme = "0.3.20"
paste = "1.0.14"
rps_rs = {{ path = "{1}", features = ["rpsl_dylib"] }}

[dependencies.macro_utils]
path = "{1}/macro_utils"

[lib]
crate-type = ["cdylib"]
path = "{2}"
"#, src_file_stem, render_graph_lib_dir, src_path);

    let tmp_src_dir = tmp_dir.join(r"src");
    std::fs::create_dir_all(&tmp_src_dir).unwrap();

    let tmp_toml = tmp_dir.join("Cargo.toml");
    if let Ok(mut toml_file) = std::fs::File::create(&tmp_toml) {
        toml_file.write_all(toml_text.as_bytes()).unwrap();
    }

    // Run Cargo to build temp project:
    let mut cargo_args: Vec<String> = args.cargo_args;
    cargo_args.insert(0, "build".into());
    cargo_args.push("--manifest-path".into());
    cargo_args.push(tmp_toml.to_str().unwrap().into());

    print!("Executing 'cargo");
    for arg in &cargo_args {
        print!(" {}", arg);
    }
    println!("'");

    let cargo_result = Command::new("cargo").current_dir(&tmp_dir).args(&cargo_args).status();

    match cargo_result {
        Ok(status) => {
            println!("Build succeeded. {}", status);
        },
        Err(e) => {
            println!("Build failed. {}", e);
        }
    }

    if args.out_dir.is_some() || args.out_file.is_some()
    {
        let is_release_build = cargo_args.contains(&String::from("--release"));

        let target_dir = match env::var("CARGO_TARGET_DIR") {
            Ok(dir) => dir,
            _ => tmp_dir.join("target").to_str().unwrap().into(),
        };

        let binary_name = format!("{}.dll", src_file_stem);

        let target_file = std::path::PathBuf::from(target_dir).join(
            if is_release_build { "release" } else { "debug" }).join(&binary_name);

        println!("Target file: '{}'", target_file.to_str().unwrap());

        if let Some(out_file) = &args.out_file {
            if let Some(out_dir) = std::path::PathBuf::from(&out_file).parent() {
                let _ = fs::create_dir_all(out_dir);
            }
            fs::copy(target_file, out_file).expect("Failed to copy target binary!");
        }
        else if let Some(out_dir) = &args.out_dir {
            let mut out_file = out_dir.clone();
            out_file.push_str("/");
            out_file.push_str(&binary_name);

            let _ = fs::create_dir_all(out_dir);

            fs::copy( target_file, out_file).expect("Failed to copy target binary to target directory!");
        }
    }

    if args.expand_macros {
        cargo_args[0] = "expand".into();

        print!("Executing 'cargo");
        for arg in &cargo_args {
            print!(" {}", arg);
        }
        println!("'");

        let cargo_result = Command::new("cargo").current_dir(&tmp_dir).args(&cargo_args).status();

        match cargo_result {
            Ok(status) => {
                println!("Expand succeeded. {}", status);
            },
            Err(e) => {
                println!("Expand failed. {}", e);
            }
        }
    }

}
