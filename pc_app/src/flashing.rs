use std::os::windows::process::CommandExt;
use crate::app_state::AppState;
use crate::utilities::{get_app_directory, get_project_directory, fetch_latest_commit, read_local_version, save_local_version, find_project_dir};
use std::path::{Path, PathBuf};
use std::io::{BufRead, Cursor};
use std::process::Command;
use std::thread;
use std::fs;
use zip::ZipArchive;
use std::sync::Arc;

pub fn flash_arduino(app_state: &Arc<AppState>) {
    let app_state_clone = app_state.clone();

    thread::spawn(move || {
        app_state_clone.set_flashing(true);
        app_state_clone.log_message("Starting Arduino flashing process...");

        let python_exe = match check_existing_python() {
            Some(python) => {
                app_state_clone.log_message("Python already installed.");
                python
            },
            None => match download_and_setup_python(&app_state_clone) {
                Ok(python) => {
                    app_state_clone.log_message("Python installed successfully.");
                    python
                },
                Err(e) => {
                    app_state_clone.log_message(&format!("Error setting up Python: {}", e));
                    app_state_clone.set_flashing(false);
                    return;
                }
            },
        };

        if !check_existing_pip(&python_exe) {
            if let Err(e) = bootstrap_pip(&python_exe, &app_state_clone) {
                app_state_clone.log_message(&format!("Error setting up pip: {}", e));
                app_state_clone.set_flashing(false);
                return;
            } else {
                app_state_clone.log_message("Pip installed successfully.");
            }
        } else {
            app_state_clone.log_message("Pip already installed.");
        }

        if !check_existing_platformio(&python_exe) {
            if let Err(e) = install_platformio(&app_state_clone) {
                app_state_clone.log_message(&format!("Error installing PlatformIO: {}", e));
                app_state_clone.set_flashing(false);
                return;
            } else {
                app_state_clone.log_message("PlatformIO installed successfully.");
            }
        } else {
            app_state_clone.log_message("PlatformIO already installed.");
        }

        match prepare_platformio_project(&app_state_clone) {
            Ok(project_dir) => {
                app_state_clone.log_message("Flashing Arduino...");

                let output = Command::new(&python_exe)
                    .current_dir(&project_dir)
                    .arg("-m")
                    .arg("platformio")
                    .arg("run")
                    .arg("--target")
                    .arg("upload")
                    .creation_flags(0x08000000) // CREATE_NO_WINDOW
                    .stdout(std::process::Stdio::piped())
                    .stderr(std::process::Stdio::piped())
                    .spawn();

                match output {
                    Ok(mut child) => {
                        if let Some(stdout) = child.stdout.take() {
                            let reader = std::io::BufReader::new(stdout);
                            for line in reader.lines() {
                                if let Ok(line) = line {
                                    if !line.contains("warning:") && !line.contains("-W") {
                                        app_state_clone.log_message(&line.clone());
                                    }
                                }
                            }
                        }

                        if let Ok(status) = child.wait() {
                            if status.success() {
                                app_state_clone.log_message("Arduino flashed successfully.");
                                app_state_clone.set_flashing(false);
                                app_state_clone.update_notification();
                            } else {
                                app_state_clone.log_message("Flash failed. Check log for details.");
                                app_state_clone.set_flashing(false);
                            }
                        }
                    }
                    Err(e) => {
                        app_state_clone.log_message(&format!(
                            "Failed to execute platformio: {:?}\nPython path: {:?}\nProject dir: {:?}",
                            e, python_exe, project_dir
                        ));
                        app_state_clone.set_flashing(false);
                    }
                }
            }
            Err(e) => {
                app_state_clone.log_message(&format!("Error preparing PlatformIO project: {}", e));
            }
        }
    });
}

fn check_existing_python() -> Option<PathBuf> {
    let python_dir = get_app_directory().join("python");
    let python_exe = python_dir.join("python.exe");
    if python_exe.exists() && python_dir.join("Lib").exists() {
        Some(python_exe)
    } else {
        None
    }
}

fn check_existing_pip(python_exe: &Path) -> bool {
    let output = Command::new(python_exe)
        .arg("-m")
        .arg("pip")
        .arg("--version")
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .creation_flags(0x08000000)
        .status();
    output.map_or(false, |status| status.success())
}

fn check_existing_platformio(python_exe: &Path) -> bool {
    let output = Command::new(python_exe)
        .arg("-m")
        .arg("pip")
        .arg("show")
        .arg("platformio")
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .creation_flags(0x08000000)
        .status();
    output.map_or(false, |status| status.success())
}

fn download_and_setup_python(app_state: &AppState) -> Result<PathBuf, String> {
    let python_dir = get_app_directory().join("python");
    let python_exe = python_dir.join("python.exe");

    // If Python is already set up properly, return early
    if python_exe.exists() && python_dir.join("Lib").exists() && python_dir.join("python311._pth").exists() {
        return Ok(python_exe);
    }

    // Clean up any partial installation
    if python_dir.exists() {
        fs::remove_dir_all(&python_dir).map_err(|e| format!("Failed to clean up Python directory: {}", e))?;
    }

    // Create fresh Python directory
    fs::create_dir_all(&python_dir).map_err(|e| format!("Failed to create Python directory: {}", e))?;

    // Download embedded Python distribution (includes stdlib)
    let python_url = "https://www.python.org/ftp/python/3.11.5/python-3.11.5-embed-amd64.zip";
    app_state.log_message("Downloading Python...");
    
    let response = reqwest::blocking::get(python_url).map_err(|e| format!("Failed to download Python: {}", e))?;
    let mut archive = ZipArchive::new(Cursor::new(response.bytes().map_err(|e| e.to_string())?))
        .map_err(|e| format!("Failed to parse Python ZIP archive: {}", e))?;

    // Extract Python distribution
    for i in 0..archive.len() {
        let mut file = archive.by_index(i).map_err(|e| format!("Failed to extract file from ZIP: {}", e))?;
        let out_path = python_dir.join(file.mangled_name());

        if file.is_dir() {
            fs::create_dir_all(&out_path).map_err(|e| format!("Failed to create directory: {}", e))?;
        } else {
            if let Some(parent) = out_path.parent() {
                fs::create_dir_all(parent).map_err(|e| format!("Failed to create parent directory: {}", e))?;
            }
            let mut outfile = fs::File::create(&out_path).map_err(|e| format!("Failed to create file: {}", e))?;
            std::io::copy(&mut file, &mut outfile).map_err(|e| format!("Failed to write file: {}", e))?;
        }
    }

    // Create/modify python3XX._pth file to enable pip installation
    let pth_file_path = python_dir.join("python311._pth");
    let pth_contents = "python311.zip\n.\nLib/site-packages\nimport site";
    fs::write(&pth_file_path, pth_contents).map_err(|e| format!("Failed to write _pth file: {}", e))?;

    // Create empty Lib/site-packages directory
    fs::create_dir_all(python_dir.join("Lib").join("site-packages"))
        .map_err(|e| format!("Failed to create site-packages directory: {}", e))?;

    Ok(python_exe)
}

fn bootstrap_pip(python_exe: &Path, app_state: &Arc<AppState>) -> Result<(), String> {
    let get_pip_url = "https://bootstrap.pypa.io/get-pip.py";
    let get_pip_script = python_exe.parent().unwrap().join("get-pip.py");

    app_state.log_message("Bootstrapping pip...");

    // Download get-pip.py
    app_state.log_message("Downloading get-pip.py...");
    let response = reqwest::blocking::get(get_pip_url)
        .map_err(|e| format!("Failed to download get-pip.py: {}", e))?;
    
    fs::write(&get_pip_script, response.bytes().map_err(|e| e.to_string())?)
        .map_err(|e| format!("Failed to write get-pip.py: {}", e))?;

    // Run get-pip.py
    app_state.log_message("Executing get-pip.py...");
    let output = Command::new(python_exe)
        .arg(&get_pip_script)
        .arg("--no-warn-script-location")
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .creation_flags(0x08000000)
        .output()
        .map_err(|e| format!("Failed to execute get-pip.py: {:?}", e))?;

    // Clean up get-pip.py regardless of success
    let _ = fs::remove_file(&get_pip_script);

    if output.status.success() {
        app_state.log_message("Pip install files cleaned up.");
        Ok(())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        app_state.log_message(&format!("Failed to bootstrap pip:\n{}", stderr));
        Err("Failed to bootstrap pip.".to_string())
    }
}

fn install_platformio(app_state: &AppState) -> Result<PathBuf, String> {
    let python_exe = download_and_setup_python(app_state)?;

    if check_existing_platformio(&python_exe) {
        app_state.log_message("PlatformIO is already installed.");
        return Ok(python_exe);
    }

    app_state.log_message("Installing PlatformIO...");

    let output = Command::new(&python_exe)
        .arg("-m")
        .arg("pip")
        .arg("install")
        .arg("platformio")
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .creation_flags(0x08000000)
        .output()
        .map_err(|e| format!("Failed to execute pip: {:?}", e))?;

    if output.status.success() {
        app_state.log_message("PlatformIO installed successfully.");
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        app_state.log_message(&format!("PlatformIO installation failed:\n{}", stderr));
        return Err("Failed to install PlatformIO".to_string());
    }

    Ok(python_exe)
}

fn prepare_platformio_project(app_state: &Arc<AppState>) -> Result<PathBuf, String> {
    let base_project_dir = get_project_directory();
    let repo_url = "FugLong/Nano33_PC_Head_Tracker";
    
    // Compare latest and local versions if local exists
    //Download if necessary
    let latest_version = fetch_latest_commit(repo_url).map_err(|e| {
        app_state.log_message(&format!("Failed to fetch latest project version: {}", e));
        e
    })?;
    
    let local_version = read_local_version();
    
    if local_version.as_deref() == Some(&latest_version) {
        app_state.log_message("Project is up-to-date.");
        return find_project_dir(&get_project_directory())
            .ok_or("Failed to locate cached project".to_string());
    }
    
    app_state.log_message(&format!("Updating to version: {}...", latest_version));

    // Clean up any existing partial project
    if base_project_dir.exists() {
        fs::remove_dir_all(&base_project_dir).map_err(|e| format!("Failed to clean up project directory: {}", e))?;
    }

    fs::create_dir_all(&base_project_dir).map_err(|e| format!("Failed to create project directory: {}", e))?;

    let repo_url = "https://github.com/FugLong/Nano33_PC_Head_Tracker/archive/refs/heads/V2_Update.zip";
    app_state.log_message(&format!("Downloading from: {}", repo_url));
    
    let response = reqwest::blocking::get(repo_url).map_err(|e| format!("Failed to download project: {}", e))?;
    
    if !response.status().is_success() {
        return Err(format!("Failed to download project: HTTP {}", response.status()));
    }

    let bytes = response.bytes().map_err(|e| format!("Failed to get response bytes: {}", e))?;
    app_state.log_message(&format!("Downloaded {} bytes", bytes.len()));

    let mut archive = ZipArchive::new(Cursor::new(bytes))
        .map_err(|e| format!("Failed to parse ZIP archive: {}", e))?;

    app_state.log_message(&format!("ZIP archive contains {} files", archive.len()));

    // Extract files
    for i in 0..archive.len() {
        let mut file = archive.by_index(i).map_err(|e| format!("Failed to extract file from ZIP: {}", e))?;
        
        let file_name = file.name().to_string(); // Immutable borrow ends here
        let out_path = base_project_dir.join(&file_name);
        
        //log.lock().unwrap().push(format!("Extracting: {}", file_name));

        if file.is_dir() {
            fs::create_dir_all(&out_path).map_err(|e| format!("Failed to create directory {}: {}", file_name, e))?;
        } else {
            if let Some(parent) = out_path.parent() {
                fs::create_dir_all(parent).map_err(|e| format!("Failed to create parent directory for {}: {}", file_name, e))?;
            }
            let mut outfile = fs::File::create(&out_path)
                .map_err(|e| format!("Failed to create file {}: {}", file_name, e))?;
            std::io::copy(&mut file, &mut outfile)
                .map_err(|e| format!("Failed to write file {}: {}", file_name, e))?;
        }
    }

    // Verify the extraction using `find_project_dir`
    if let Some(project_dir) = find_project_dir(&base_project_dir) {
        app_state.log_message(&format!("Project directory verified successfully: {:?}", project_dir));
        save_local_version(&latest_version).map_err(|e| {
            app_state.log_message(&format!("Failed to save local version: {}", e));
            e
        })?;
        Ok(project_dir)
    } else {
        // Debugging: List contents of the base directory
        app_state.log_message("Failed to find project directory after extraction. Contents:");
        if let Ok(entries) = fs::read_dir(&base_project_dir) {
            for entry in entries {
                if let Ok(entry) = entry {
                    app_state.log_message(&format!("  {:?}", entry.path()));
                }
            }
        }
        Err("Failed to find valid project directory after extraction.".to_string())
    }
}