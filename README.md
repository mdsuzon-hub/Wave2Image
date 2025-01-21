

# 🌊 **Wave2Image**:

**Project Overview:**

**Wave2Image** project! 🌟 is a software that turns images into audio 🎧 waves and then converts those waves back into the original image. It's inspired by the way NOAA weather satellites send images. One of the main goal of this project is to keep the image quality intact while it's being converted.

This project is a part of the **Data Structure Project** at **Daffodil International University**

---

## 🔧 **Installation Guide for Windows**

### 🚀 **Install MSYS2**

Download and install MSYS2 from the official [MSYS2 Website](https://www.msys2.org/) 💻

---

### 🔄 **Update MSYS2 Package**

Update the MSYS2 package to have the latest tools and libraries. Run on MSYS2 terminal:

```bash
pacman -Syu
```

Update the MSYS2 core system and packages. 🆙

---

### 🖌️ **Install GTK and Glade**

Install GTK is for the graphical interface, and Gladefor design it.  Run:

```bash
pacman -S mingw-w64-x86_64-gtk3
pacman -S mingw-w64-x86_64-glade
```

Ready to show the user interface! 🎨

---

### 🛠️ **Install GCC Compiler**

Next, install GCC, for building the project. Enter the following command:

```bash
pacman -S mingw-w64-x86_64-gcc
```

GCC will compile the C code and link all necessary libraries. ⏳

---

### ⚡ **Compilation**

All set to compile the project! Navigate to the project folder where the `main.c` file is located, and use this command:

```bash
gcc -o wave2img main.c -lpng -lm `pkg-config --cflags --libs gtk+-3.0`
```

This will create the **wave2img** executable. 🏗️

---

### 🚀 **Run the Software**

After successful compilation, run the executable to start the conversion from image to audio wave and vice versa.

![Wave2Image](https://raw.githubusercontent.com/mdsuzon-hub/Wave2Image/main/src/assets/wave2img.png)





## 📝 **License**

This project is licensed under the CC BY-NC License. See the [LICENSE](LICENSE) file for more details.


## 🤝 **Acknowledgements**

- Inspired by the NOAA weather satellite image transmission system.
- Developed as part of the **Data Structure Project** at **Daffodil International University**. 📚
- Special thanks to the open-source community for making tools like MSYS2, GTK, and GCC available! 🌍

### **Project Contributors**:
- **Project Manager**: MD Mainul Islam
- **Front-End Developer**: MD Tamim Hossan
- **Back-End Developer**: MD Boni Amin
- **Data Structure Architect**: MD Suzon Mia


---

Feel free to contribute, suggest improvements, or explore this project further! 🚀
