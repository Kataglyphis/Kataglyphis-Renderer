@ECHO ON
C:/VulkanSDK/1.2.176.1/Bin32/glslc.exe ../Resources/Shader/raytrace.rgen -o ../Resources/Shader/raytrace.rgen.spv
C:/VulkanSDK/1.2.176.1/Bin32/glslc.exe ../Resources/Shader/raytrace.rchit -o ../Resources/Shader/raytrace.rchit.spv
C:/VulkanSDK/1.2.176.1/Bin32/glslc.exe ../Resources/Shader/raytrace.rmiss -o ../Resources/Shader/raytrace.rmiss.spv
C:/VulkanSDK/1.2.176.1/Bin32/glslc.exe ../Resources/Shader/raytraceShadow.rmiss -o ../Resources/Shader/raytraceShadow.rmiss.spv