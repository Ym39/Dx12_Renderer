#include <string>
#include <fstream>
#include "VMDFileData.h"

bool ReadHeader(VMDFileData& data, std::ifstream& file)
{
	file.read(reinterpret_cast<char*>(&data.header.header), 30);

	std::string headerStr = data.header.header;

	if (headerStr != "Vocaloid Motion Data 0002" &&
		headerStr != "Vocaloid Motion Data")
	{
		return false;
	}

	file.read(reinterpret_cast<char*>(&data.header.modelName), 20);

	return true;
}

bool ReadMotion(VMDFileData& data, std::ifstream& file)
{
	unsigned int count = 0;
	file.read(reinterpret_cast<char*>(&count), 4);

	data.motions.resize(count);
	for (auto& motion : data.motions)
	{
		UnicodeUtil::ReadJISToWString(file, motion.boneName, 15);
		//file.read(reinterpret_cast<char*>(&motion.boneName), 15);
		file.read(reinterpret_cast<char*>(&motion.frame), 4);
		file.read(reinterpret_cast<char*>(&motion.translate), 12);
		file.read(reinterpret_cast<char*>(&motion.quaternion), 16);
		file.read(reinterpret_cast<char*>(motion.interpolation), 64);
	}

	return true;
}

bool ReadMorph(VMDFileData& data, std::ifstream& file)
{
	unsigned int count = 0;
	file.read(reinterpret_cast<char*>(&count), 4);

	data.morphs.resize(count);
	for (auto& morph : data.morphs)
	{
		//file.read(reinterpret_cast<char*>(&morph.blendShapeName), 15);
		UnicodeUtil::ReadJISToWString(file, morph.blendShapeName, 15);
		file.read(reinterpret_cast<char*>(&morph.frame), 4);
		file.read(reinterpret_cast<char*>(&morph.weight), 4);
	}

	return true;
}

bool ReadCamera(VMDFileData& data, std::ifstream& file)
{
	unsigned int count = 0;
	file.read(reinterpret_cast<char*>(&count), 4);

	data.cameras.resize(count);
	for (auto& camera : data.cameras)
	{
		file.read(reinterpret_cast<char*>(&camera.frame), 4);
		file.read(reinterpret_cast<char*>(&camera.distance), 4);
		file.read(reinterpret_cast<char*>(&camera.interest), 12);
		file.read(reinterpret_cast<char*>(&camera.rotate), 12);
		file.read(reinterpret_cast<char*>(camera.interpolation), 24);
		file.read(reinterpret_cast<char*>(&camera.fov), 4);
		file.read(reinterpret_cast<char*>(&camera.isPerspective), 1);
	}

	return true;
}

bool ReadLight(VMDFileData& data, std::ifstream& file)
{
	unsigned int count = 0;
	file.read(reinterpret_cast<char*>(&count), 4);

	data.lights.resize(count);
	for (auto& light : data.lights)
	{
		file.read(reinterpret_cast<char*>(&light.frame), 4);
		file.read(reinterpret_cast<char*>(&light.color), 12);
		file.read(reinterpret_cast<char*>(&light.position), 12);
	}

	return true;
}

bool ReadShadow(VMDFileData& data, std::ifstream& file)
{
	unsigned int count = 0;
	file.read(reinterpret_cast<char*>(&count), 4);

	data.shadows.resize(count);
	for (auto& shadow : data.shadows)
	{
		file.read(reinterpret_cast<char*>(&shadow.frame), 4);
		file.read(reinterpret_cast<char*>(&shadow.shadowType), 1);
		file.read(reinterpret_cast<char*>(&shadow.distance), 4);
	}

	return true;
}

bool ReadIK(VMDFileData& data, std::ifstream& file)
{
	unsigned int count = 0;
	file.read(reinterpret_cast<char*>(&count), 4);

	data.iks.resize(count);
	for (auto& ik : data.iks)
	{
		file.read(reinterpret_cast<char*>(&ik.frame), 4);
		file.read(reinterpret_cast<char*>(&ik.show), 1);

		unsigned int ikInfoCount = 0;
		file.read(reinterpret_cast<char*>(&ikInfoCount), 4);

		ik.ikInfos.resize(ikInfoCount);
		for (auto& ikInfo : ik.ikInfos)
		{
			//file.read(ikInfo.name, 20);
			UnicodeUtil::ReadJISToWString(file, ikInfo.name, 20);
			file.read(reinterpret_cast<char*>(&ikInfo.enable), 1);
		}
	}

	return true;
}

bool LoadVMDFile(const std::wstring& filePath, VMDFileData& fileData)
{
	if (filePath.empty() == true)
	{
		return false;
	}

	std::ifstream vmdFile{ filePath, (std::ios::binary | std::ios::in) };

	if (vmdFile.fail() == true)
	{
		vmdFile.close();
		return false;
	}

	bool result = ReadHeader(fileData, vmdFile);
	if (result == false)
	{
		return false;
	}

	result = ReadMotion(fileData, vmdFile);
	if (result == false)
	{
		return false;
	}

	result = ReadMorph(fileData, vmdFile);
	if (result == false)
	{
		return false;
	}

	result = ReadCamera(fileData, vmdFile);
	if (result == false)
	{
		return false;
	}

	result = ReadLight(fileData, vmdFile);
	if (result == false)
	{
		return false;
	}

	result = ReadShadow(fileData, vmdFile);
	if (result == false)
	{
		return false;
	}

	result = ReadIK(fileData, vmdFile);
	if (result == false)
	{
		return false;
	}

	vmdFile.close();

	return true;
}
