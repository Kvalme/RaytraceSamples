call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"
nvcc -I"C:\ProgramData\NVIDIA Corporation\OptiX SDK 5.0.1\include" --ptx triangle_mesh.cu