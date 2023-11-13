#include "PmxFileData.h"
#include "UnicodeUtil.h"

bool ReadHeader(PMXFileData& data, std::ifstream& file)
{
	file.read(reinterpret_cast<char*>(data.header.magic.data()), data.header.magic.size());
	if (data.header.magic != PMX_MAGIC_NUMBER)
	{
		return false;
	}

	file.read(reinterpret_cast<char*>(&data.header.version), sizeof(data.header.version));
	file.read(reinterpret_cast<char*>(&data.header.dataLength), sizeof(data.header.dataLength));

	file.read(reinterpret_cast<char*>(&data.header.textEncoding), sizeof(data.header.textEncoding));
	file.read(reinterpret_cast<char*>(&data.header.addUVNum), sizeof(data.header.addUVNum));
	file.read(reinterpret_cast<char*>(&data.header.vertexIndexSize), sizeof(data.header.vertexIndexSize));
	file.read(reinterpret_cast<char*>(&data.header.textureIndexSize), sizeof(data.header.textureIndexSize));
	file.read(reinterpret_cast<char*>(&data.header.materialIndexSize), sizeof(data.header.materialIndexSize));
	file.read(reinterpret_cast<char*>(&data.header.boneIndexSize), sizeof(data.header.boneIndexSize));
	file.read(reinterpret_cast<char*>(&data.header.morphIndexSize), sizeof(data.header.morphIndexSize));
	file.read(reinterpret_cast<char*>(&data.header.rigidBodyIndexSize), sizeof(data.header.rigidBodyIndexSize));

	return true;
}

bool ReadString(int encoding, std::string& string, std::ifstream& file)
{
	unsigned int bufSize;
	file.read(reinterpret_cast<char*>(&bufSize), sizeof(bufSize));

	if (bufSize > 0)
	{
		if (encoding == 0) // utf-16
		{
			std::u16string utf16Str(bufSize, u'\0');
			file.read(reinterpret_cast<char*>(&utf16Str[0]), utf16Str.size());
			if (UnicodeUtil::ConvU16ToU8(utf16Str, string) == false)
			{
				return false;
			}
		}
		else if (encoding == 1) //utf-8
		{
			std::string utf8Str(bufSize, u'\0');
			file.read(reinterpret_cast<char*>(&utf8Str[0]), utf8Str.size());
			string = utf8Str;
		}
	}

	return true;
}

bool ReadModelInfo(PMXFileData& data, std::ifstream& file)
{
	UnicodeUtil::GetPMXStringUTF16(file, data.modelInfo.modelName);
	UnicodeUtil::GetPMXStringUTF8(file, data.modelInfo.englishModelName);
	UnicodeUtil::GetPMXStringUTF16(file, data.modelInfo.comment);
	UnicodeUtil::GetPMXStringUTF8(file, data.modelInfo.englishComment);

	return true;
}

bool ReadVertex(PMXFileData& data, std::ifstream& file)
{
	unsigned int vertexCount;
	file.read(reinterpret_cast<char*>(&vertexCount), 4);
	data.vertices.resize(vertexCount);

	for (auto& vertex : data.vertices)
	{
		file.read(reinterpret_cast<char*>(&vertex.position), 12);
		file.read(reinterpret_cast<char*>(&vertex.normal), 12);
		file.read(reinterpret_cast<char*>(&vertex.uv), 8);

		for (int i = 0; i < data.header.addUVNum; i++)
		{
			file.read(reinterpret_cast<char*>(&vertex.additionalUV[i]), 16);
		}

		file.read(reinterpret_cast<char*>(&vertex.weightType), 1);

		const unsigned char boneIndexSize = data.header.boneIndexSize;

		switch (vertex.weightType)
		{
		case PMXVertexWeight::BDEF1:
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[0]), boneIndexSize);
			break;
		case PMXVertexWeight::BDEF2:
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[0]), boneIndexSize);
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[1]), boneIndexSize);
			file.read(reinterpret_cast<char*>(&vertex.boneWeights[0]), 4);
			break;
		case PMXVertexWeight::BDEF4:
		case PMXVertexWeight::QDEF:
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[0]), boneIndexSize);
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[1]), boneIndexSize);
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[2]), boneIndexSize);
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[3]), boneIndexSize);
			file.read(reinterpret_cast<char*>(&vertex.boneWeights[0]), 4);
			file.read(reinterpret_cast<char*>(&vertex.boneWeights[1]), 4);
			file.read(reinterpret_cast<char*>(&vertex.boneWeights[2]), 4);
			file.read(reinterpret_cast<char*>(&vertex.boneWeights[3]), 4);
			break;
		case PMXVertexWeight::SDEF:
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[0]), boneIndexSize);
			file.read(reinterpret_cast<char*>(&vertex.boneIndices[1]), boneIndexSize);
			file.read(reinterpret_cast<char*>(&vertex.boneWeights[0]), 4);
			file.read(reinterpret_cast<char*>(&vertex.sdefC), 12);
			file.read(reinterpret_cast<char*>(&vertex.sdefR0), 12);
			file.read(reinterpret_cast<char*>(&vertex.sdefR1), 12);
			break;
		default:
			return false;
		}

		file.read(reinterpret_cast<char*>(&vertex.edgeMag), 4);
	}

	return true;
}

bool ReadFace(PMXFileData& data, std::ifstream& file)
{
	int faceCount = 0;
	file.read(reinterpret_cast<char*>(&faceCount), 4);

	faceCount /= 3;
	data.faces.resize(faceCount);

	switch (data.header.vertexIndexSize)
	{
	case 1:
	{
		std::vector<uint8_t> vertices(faceCount * 3);
		file.read(reinterpret_cast<char*>(vertices.data()), sizeof(uint8_t) * vertices.size());
		for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++)
		{
			data.faces[faceIdx].vertices[0] = vertices[faceIdx * 3 + 0];
			data.faces[faceIdx].vertices[1] = vertices[faceIdx * 3 + 1];
			data.faces[faceIdx].vertices[2] = vertices[faceIdx * 3 + 2];
		}
	}
		break;
	case 2:
	{
		std::vector<uint16_t> vertices(faceCount * 3);
		file.read(reinterpret_cast<char*>(vertices.data()), sizeof(uint16_t) * vertices.size());
		for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++)
		{
			data.faces[faceIdx].vertices[0] = vertices[faceIdx * 3 + 0];
			data.faces[faceIdx].vertices[1] = vertices[faceIdx * 3 + 1];
			data.faces[faceIdx].vertices[2] = vertices[faceIdx * 3 + 2];
		}
	}
		break;
	case 4:
	{
		std::vector<uint32_t> vertices(faceCount * 3);
		file.read(reinterpret_cast<char*>(vertices.data()), sizeof(uint32_t) * vertices.size());
		for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++)
		{
			data.faces[faceIdx].vertices[0] = vertices[faceIdx * 3 + 0];
			data.faces[faceIdx].vertices[1] = vertices[faceIdx * 3 + 1];
			data.faces[faceIdx].vertices[2] = vertices[faceIdx * 3 + 2];
		}
	}
		break;
	default:
			return false;
	}

	return true;
}

bool ReadTextures(PMXFileData& data, std::ifstream& file)
{
	unsigned int numOfTexture = 0;
	file.read(reinterpret_cast<char*>(&numOfTexture), 4);

	data.textures.resize(numOfTexture);

	for (auto& texture : data.textures)
	{
		UnicodeUtil::GetPMXStringUTF16(file, texture.textureName);
	}

	return true;
}

bool ReadMaterial(PMXFileData& data, std::ifstream& file)
{
	int numOfMaterial = 0;
	file.read(reinterpret_cast<char*>(&numOfMaterial), 4);

	data.materials.resize(numOfMaterial);

	for (auto& mat : data.materials)
	{
		UnicodeUtil::GetPMXStringUTF16(file, mat.name);
		UnicodeUtil::GetPMXStringUTF8(file, mat.englishName);

		file.read(reinterpret_cast<char*>(&mat.diffuse), 16);
		file.read(reinterpret_cast<char*>(&mat.specular), 12);
		file.read(reinterpret_cast<char*>(&mat.specularPower), 4);
		file.read(reinterpret_cast<char*>(&mat.ambient), 12);

		file.read(reinterpret_cast<char*>(&mat.drawMode), 1);

		file.read(reinterpret_cast<char*>(&mat.edgeColor), 16);
		file.read(reinterpret_cast<char*>(&mat.edgeSize), 4);

		file.read(reinterpret_cast<char*>(&mat.textureIndex), data.header.textureIndexSize);
		file.read(reinterpret_cast<char*>(&mat.sphereTextureIndex), data.header.textureIndexSize);
		file.read(reinterpret_cast<char*>(&mat.sphereMode), 1);

		file.read(reinterpret_cast<char*>(&mat.toonMode), 1);

		if (mat.toonMode == PMXToonMode::Separate)
		{
			file.read(reinterpret_cast<char*>(&mat.toonTextureIndex), data.header.textureIndexSize);
		}
		else if (mat.toonMode == PMXToonMode::Common)
		{
			file.read(reinterpret_cast<char*>(&mat.toonTextureIndex), 1);
		}
		else
		{
			return false;
		}

		UnicodeUtil::GetPMXStringUTF16(file, mat.memo);

		file.read(reinterpret_cast<char*>(&mat.numFaceVertices), 4);
	}

	return true;
}

bool ReadBone(PMXFileData& data, std::ifstream& file)
{
	unsigned int numOfBone;
	file.read(reinterpret_cast<char*>(&numOfBone), 4);

	data.bones.resize(numOfBone);

	for (auto& bone : data.bones)
	{
		UnicodeUtil::GetPMXStringUTF16(file, bone.name);
		UnicodeUtil::GetPMXStringUTF8(file, bone.englishName);

		file.read(reinterpret_cast<char*>(&bone.position), 12);
		file.read(reinterpret_cast<char*>(&bone.parentBoneIndex), data.header.boneIndexSize);
		file.read(reinterpret_cast<char*>(&bone.deformDepth), 4);

		file.read(reinterpret_cast<char*>(&bone.boneFlag), 2);

		if (((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::TargetShowMode) == 0)
		{
			file.read(reinterpret_cast<char*>(&bone.positionOffset), 12);
		}
		else
		{
			file.read(reinterpret_cast<char*>(&bone.linkBoneIndex), data.header.boneIndexSize);
		}

		if (((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::AppendRotate) ||
			((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::AppendTranslate))
		{
			file.read(reinterpret_cast<char*>(&bone.appendBoneIndex), data.header.boneIndexSize);
			file.read(reinterpret_cast<char*>(&bone.appendWeight), 4);
		}

		if ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::FixedAxis)
		{
			file.read(reinterpret_cast<char*>(&bone.fixedAxis), 12);
		}

		if ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::LocalAxis)
		{
			file.read(reinterpret_cast<char*>(&bone.localXAxis), 12);
			file.read(reinterpret_cast<char*>(&bone.localZAxis), 12);
		}

		if ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::DeformOuterParent)
		{
			file.read(reinterpret_cast<char*>(&bone.keyValue), 4);
		}

		if ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::IK)
		{
			file.read(reinterpret_cast<char*>(&bone.ikTargetBoneIndex), data.header.boneIndexSize);
			file.read(reinterpret_cast<char*>(&bone.ikIterationCount), 4);
			file.read(reinterpret_cast<char*>(&bone.ikLimit), 4);

			unsigned int linkCount = 0;
			file.read(reinterpret_cast<char*>(&linkCount), 4);

			bone.ikLinks.resize(linkCount);
			for (auto& ikLink : bone.ikLinks)
			{
				file.read(reinterpret_cast<char*>(&ikLink.ikBoneIndex), data.header.boneIndexSize);
				file.read(reinterpret_cast<char*>(&ikLink.enableLimit), 1);

				if (ikLink.enableLimit != 0)
				{
					file.read(reinterpret_cast<char*>(&ikLink.limitMin), 12);
					file.read(reinterpret_cast<char*>(&ikLink.limitMax), 12);
				}
			}
		}
	}

	return true;
}

bool ReadMorph(PMXFileData& data, std::ifstream& file)
{
	unsigned int numOfMorph = 0;
	file.read(reinterpret_cast<char*>(&numOfMorph), 4);

	data.morphs.resize(numOfMorph);

	for (auto& morph : data.morphs)
	{
		UnicodeUtil::GetPMXStringUTF16(file, morph.name);
		UnicodeUtil::GetPMXStringUTF8(file, morph.englishName);

		file.read(reinterpret_cast<char*>(&morph.controlPanel), 1);
		file.read(reinterpret_cast<char*>(&morph.morphType), 1);

		unsigned int dataCount;
		file.read(reinterpret_cast<char*>(&dataCount), 4);

		if (morph.morphType == PMXMorphType::Position)
		{
			morph.positionMorph.resize(dataCount);
			for (auto& morphData : morph.positionMorph)
			{
				file.read(reinterpret_cast<char*>(&morphData.vertexIndex), data.header.vertexIndexSize);
				file.read(reinterpret_cast<char*>(&morphData.position), 12);
			}
		}
		else if (morph.morphType == PMXMorphType::UV ||
			     morph.morphType == PMXMorphType::AddUV1 || 
			     morph.morphType == PMXMorphType::AddUV2 || 
			     morph.morphType == PMXMorphType::AddUV3 || 
			     morph.morphType == PMXMorphType::AddUV4)
		{
			morph.uvMorph.resize(dataCount);
			for (auto& morphData : morph.uvMorph)
			{
				file.read(reinterpret_cast<char*>(&morphData.vertexIndex), data.header.vertexIndexSize);
				file.read(reinterpret_cast<char*>(&morphData.uv), 16);
			}
		}
		else if (morph.morphType == PMXMorphType::Bone)
		{
			morph.boneMorph.resize(dataCount);
			for (auto& morphData : morph.boneMorph)
			{
				file.read(reinterpret_cast<char*>(&morphData.boneIndex), data.header.boneIndexSize);
				file.read(reinterpret_cast<char*>(&morphData.position), 12);
				file.read(reinterpret_cast<char*>(&morphData.quaternion), 16);
			}
		}
		else if (morph.morphType == PMXMorphType::Material)
		{
			morph.materialMorph.resize(dataCount);
			for (auto& morphData : morph.materialMorph)
			{
				file.read(reinterpret_cast<char*>(&morphData.materialIndex), data.header.materialIndexSize);
				file.read(reinterpret_cast<char*>(&morphData.opType), 1);
				file.read(reinterpret_cast<char*>(&morphData.diffuse), 16);
				file.read(reinterpret_cast<char*>(&morphData.specular), 12);
				file.read(reinterpret_cast<char*>(&morphData.specularPower), 4);
				file.read(reinterpret_cast<char*>(&morphData.ambient), 12);
				file.read(reinterpret_cast<char*>(&morphData.edgeColor), 16);
				file.read(reinterpret_cast<char*>(&morphData.edgeSize), 4);
				file.read(reinterpret_cast<char*>(&morphData.textureFactor), 16);
				file.read(reinterpret_cast<char*>(&morphData.sphereTextureFactor), 16);
				file.read(reinterpret_cast<char*>(&morphData.toonTextureFactor), 16);
			}
		}
		else if (morph.morphType == PMXMorphType::Group)
		{
			morph.groupMorph.resize(dataCount);
			for (auto& morphData : morph.groupMorph)
			{
				file.read(reinterpret_cast<char*>(&morphData.morphIndex), data.header.morphIndexSize);
				file.read(reinterpret_cast<char*>(&morphData.weight), 4);
			}
		}
		else if (morph.morphType == PMXMorphType::Flip)
		{
			morph.flipMorph.resize(dataCount);
			for (auto& morphData : morph.flipMorph)
			{
				file.read(reinterpret_cast<char*>(&morphData.morphIndex), data.header.morphIndexSize);
				file.read(reinterpret_cast<char*>(&morphData.weight), 4);
			}
		}
		else if (morph.morphType == PMXMorphType::Impluse)
		{
			morph.impulseMorph.resize(dataCount);
			for (auto& morphData : morph.impulseMorph)
			{
				file.read(reinterpret_cast<char*>(&morphData.rigidBodyIndex), data.header.rigidBodyIndexSize);
				file.read(reinterpret_cast<char*>(&morphData.localFlag), 1);
				file.read(reinterpret_cast<char*>(&morphData.translateVelocity), 12);
				file.read(reinterpret_cast<char*>(&morphData.rotateTorque), 12);
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool ReadDisplayFrame(PMXFileData& data, std::ifstream& file)
{
	unsigned int numOfDisplayFrame = 0;
	file.read(reinterpret_cast<char*>(&numOfDisplayFrame), 4);

	data.displayFrames.resize(numOfDisplayFrame);

	for (auto& displayFrame : data.displayFrames)
	{
		UnicodeUtil::GetPMXStringUTF16(file, displayFrame.name);
		UnicodeUtil::GetPMXStringUTF8(file, displayFrame.englishName);

		file.read(reinterpret_cast<char*>(&displayFrame.flag), 1);

		unsigned int targetCount = 0;
		file.read(reinterpret_cast<char*>(&targetCount), 4);

		displayFrame.targets.resize(targetCount);
		for (auto& target : displayFrame.targets)
		{
			file.read(reinterpret_cast<char*>(&target.type), 1);
			if (target.type == PMXDisplayFrame::TargetType::BoneIndex)
			{
				file.read(reinterpret_cast<char*>(&target.index), data.header.boneIndexSize);
			}
			else if (target.type == PMXDisplayFrame::TargetType::MorphIndex)
			{
				file.read(reinterpret_cast<char*>(&target.index), data.header.morphIndexSize);
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

bool ReadRigidBody(PMXFileData& data, std::ifstream& file)
{
	unsigned int numOfRigidBody = 0;
	file.read(reinterpret_cast<char*>(&numOfRigidBody), 4);

	data.rigidBodies.resize(numOfRigidBody);

	for (auto& rigidBody : data.rigidBodies)
	{
		UnicodeUtil::GetPMXStringUTF16(file, rigidBody.name);
		UnicodeUtil::GetPMXStringUTF8(file, rigidBody.englishName);

		file.read(reinterpret_cast<char*>(&rigidBody.boneIndex), data.header.boneIndexSize);
		file.read(reinterpret_cast<char*>(&rigidBody.group), 1);
		file.read(reinterpret_cast<char*>(&rigidBody.collisionGroup), 2);
		file.read(reinterpret_cast<char*>(&rigidBody.shape), 1);
		file.read(reinterpret_cast<char*>(&rigidBody.shapeSize), 12);
		file.read(reinterpret_cast<char*>(&rigidBody.translate), 12);
		file.read(reinterpret_cast<char*>(&rigidBody.rotate), 12);
		file.read(reinterpret_cast<char*>(&rigidBody.mass), 4);
		file.read(reinterpret_cast<char*>(&rigidBody.translateDimmer), 4);
		file.read(reinterpret_cast<char*>(&rigidBody.rotateDimmer), 4);
		file.read(reinterpret_cast<char*>(&rigidBody.repulsion), 4);
		file.read(reinterpret_cast<char*>(&rigidBody.friction), 4);
		file.read(reinterpret_cast<char*>(&rigidBody.op), 1);
	}

	return true;
}

bool ReadJoint(PMXFileData& data, std::ifstream& file)
{
	unsigned int numOfJoint = 0;
	file.read(reinterpret_cast<char*>(&numOfJoint), 4);

	data.joints.resize(numOfJoint);

	for (auto& joint : data.joints)
	{
		UnicodeUtil::GetPMXStringUTF16(file, joint.name);
		UnicodeUtil::GetPMXStringUTF8(file, joint.englishName);

		file.read(reinterpret_cast<char*>(&joint.type), 1);
		file.read(reinterpret_cast<char*>(&joint.rigidBodyAIndex), data.header.rigidBodyIndexSize);
		file.read(reinterpret_cast<char*>(&joint.rigidBodyBIndex), data.header.rigidBodyIndexSize);

		file.read(reinterpret_cast<char*>(&joint.translate), 12);
		file.read(reinterpret_cast<char*>(&joint.rotate), 12);

		file.read(reinterpret_cast<char*>(&joint.translateLowerLimit), 12);
		file.read(reinterpret_cast<char*>(&joint.translateUpperLimit), 12);
		file.read(reinterpret_cast<char*>(&joint.rotateLowerLimit), 12);
		file.read(reinterpret_cast<char*>(&joint.rotateUpperLimit), 12);

		file.read(reinterpret_cast<char*>(&joint.springTranslateFactor), 12);
		file.read(reinterpret_cast<char*>(&joint.springRotateFactor), 12);
	}

	return true;
}

bool ReadSoftBody(PMXFileData& data, std::ifstream& file)
{
	unsigned int numOfSoftBody = 0;
	file.read(reinterpret_cast<char*>(&numOfSoftBody), 4);

	data.softBodies.resize(numOfSoftBody);

	for (auto& softBody : data.softBodies)
	{
		UnicodeUtil::GetPMXStringUTF16(file, softBody.name);
		UnicodeUtil::GetPMXStringUTF8(file, softBody.englishName);

		file.read(reinterpret_cast<char*>(&softBody.type), 1);

		file.read(reinterpret_cast<char*>(&softBody.materialIndex), data.header.materialIndexSize);
		file.read(reinterpret_cast<char*>(&softBody.group), 1);
		file.read(reinterpret_cast<char*>(&softBody.collisionGroup), 2);

		file.read(reinterpret_cast<char*>(&softBody.flag), 1);

		file.read(reinterpret_cast<char*>(&softBody.bLinkLength), 4);
		file.read(reinterpret_cast<char*>(&softBody.numClusters), 4);

		file.read(reinterpret_cast<char*>(&softBody.totalMass), 4);
		file.read(reinterpret_cast<char*>(&softBody.collisionMargin), 4);

		file.read(reinterpret_cast<char*>(&softBody.areoModel), 4);

		file.read(reinterpret_cast<char*>(&softBody.vcf), 4);
		file.read(reinterpret_cast<char*>(&softBody.dp), 4);
		file.read(reinterpret_cast<char*>(&softBody.dg), 4);
		file.read(reinterpret_cast<char*>(&softBody.lf) , 4);
		file.read(reinterpret_cast<char*>(&softBody.pr), 4);
		file.read(reinterpret_cast<char*>(&softBody.vc), 4);
		file.read(reinterpret_cast<char*>(&softBody.df), 4);
		file.read(reinterpret_cast<char*>(&softBody.mt), 4);
		file.read(reinterpret_cast<char*>(&softBody.chr), 4);
		file.read(reinterpret_cast<char*>(&softBody.khr), 4);
		file.read(reinterpret_cast<char*>(&softBody.shr), 4);
		file.read(reinterpret_cast<char*>(&softBody.ahr), 4);

		file.read(reinterpret_cast<char*>(&softBody.srhr_cl), 4);
		file.read(reinterpret_cast<char*>(&softBody.skhr_cl), 4);
		file.read(reinterpret_cast<char*>(&softBody.sshr_cl), 4);
		file.read(reinterpret_cast<char*>(&softBody.sr_splt_cl), 4);
		file.read(reinterpret_cast<char*>(&softBody.sk_splt_cl), 4);
		file.read(reinterpret_cast<char*>(&softBody.ss_splt_cl), 4);

		file.read(reinterpret_cast<char*>(&softBody.v_it), 4);
		file.read(reinterpret_cast<char*>(&softBody.p_it), 4);
		file.read(reinterpret_cast<char*>(&softBody.d_it), 4);
		file.read(reinterpret_cast<char*>(&softBody.c_it), 4);

		file.read(reinterpret_cast<char*>(&softBody.lst), 4);
		file.read(reinterpret_cast<char*>(&softBody.ast), 4);
		file.read(reinterpret_cast<char*>(&softBody.vst), 4);

		unsigned int anchorCount = 0;
		file.read(reinterpret_cast<char*>(&anchorCount), 4);

		softBody.anchorRigidBodies.resize(anchorCount);
		for (auto& anchor : softBody.anchorRigidBodies)
		{
			file.read(reinterpret_cast<char*>(&anchor.rigidBodyIndex), data.header.rigidBodyIndexSize);
			file.read(reinterpret_cast<char*>(&anchor.vertexIndex), data.header.vertexIndexSize);
			file.read(reinterpret_cast<char*>(&anchor.nearMode), 1);
		}

		unsigned int pinVertexCount = 0;
		file.read(reinterpret_cast<char*>(&pinVertexCount), 4);

		softBody.pinVertexIndices.resize(pinVertexCount);
		for (auto& pinVertex : softBody.pinVertexIndices)
		{
			file.read(reinterpret_cast<char*>(&pinVertex), data.header.vertexIndexSize);
		}
	}

	return true;
}

bool LoadPMXFile(const std::wstring& filePath, PMXFileData& fileData)
{
	if (filePath.empty() == true)
	{
		return false;
	}

	std::wstring folderPath{ filePath.begin(), filePath.begin() + filePath.rfind(L'\\') + 1 };

	std::ifstream pmxFile{ filePath, (std::ios::binary | std::ios::in) };
	if (pmxFile.fail())
	{
		pmxFile.close();
		return false;
	}

	bool result = ReadHeader(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadModelInfo(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadVertex(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadFace(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadTextures(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadMaterial(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadBone(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadMorph(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadDisplayFrame(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadRigidBody(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadJoint(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	result = ReadSoftBody(fileData, pmxFile);
	if (result == false)
	{
		return false;
	}

	return true;
}