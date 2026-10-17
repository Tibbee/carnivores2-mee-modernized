// AssetLoad.cpp -- TGA/WAV/text asset loading (split from Resources.cpp)
// ==========================================================================
// The menu's picture, wave and text loaders plus the Picture/SoundFX
// lifetime helpers they are built on.
// ==========================================================================

#include "Hunt.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Loaders/LoadDiagnostics.h"
#include "Loaders/TgaFormat.h"
#include "../Hunt/Audio/WaveReader.h"

bool ReadTGAFile(const std::string& path, TargaImage& tga)
{
	std::ifstream fs(path, std::ios::binary);

	if (!fs.is_open()) {
		std::cout << "Warning: Failed to open TGA file: " << path << std::endl;
		return false;
	}

	if (!fs.read(reinterpret_cast<char*>(&tga.m_Header), sizeof(TARGAINFOHEADER))) {
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"truncated TGA header", nullptr);
		return false;
	}

	// Menu pictures are 16-bit throughout, including the full-screen menu
	// images copied in Menu.cpp. Reject other depths before allocating data.
	switch (CheckTgaPictureFormat(tga.m_Header)) {
	case TgaPictureFormat::Ok:
		break;
	case TgaPictureFormat::Paletted:
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"TGA has a color palette (16-bit uncompressed truecolor required)", nullptr);
		return false;
	case TgaPictureFormat::NotTruecolor:
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"TGA is not uncompressed truecolor (16-bit uncompressed truecolor required)", nullptr);
		return false;
	case TgaPictureFormat::Not16Bit:
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"expected a 16-bit TGA (menu art is 16-bit 5551)", nullptr);
		return false;
	}
	const uint64_t size = static_cast<uint64_t>(tga.m_Header.tgaWidth) *
		tga.m_Header.tgaHeight * sizeof(uint16_t);
	if (!size || size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
		size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"invalid TGA dimensions", nullptr);
		return false;
	}
	fs.seekg(tga.m_Header.tgaIdentSize, std::ios::cur);
	const std::streampos start = fs.tellg();
	fs.seekg(0, std::ios::end);
	if (start == std::streampos(-1) || fs.tellg() - start < static_cast<std::streamoff>(size)) {
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"truncated TGA pixel data", nullptr);
		return false;
	}
	fs.seekg(start);
	std::unique_ptr<uint8_t[]> data(new uint8_t[static_cast<size_t>(size)]);
	if (!fs.read(reinterpret_cast<char*>(data.get()), static_cast<std::streamsize>(size))) {
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"could not read TGA pixel data", nullptr);
		return false;
	}
	delete[] tga.m_Data;
	tga.m_Data = data.release();
	return true;
}


bool LoadMenuBackground(const std::string& path, TargaImage& tga)
{
	if (!ReadTGAFile(path, tga)) return false;
	if (tga.m_Header.tgaWidth != 800 || tga.m_Header.tgaHeight != 600) {
		LoadDiagnostics::Instance().Report("menu background", path.c_str(),
			"full-screen menu art must be 800x600", nullptr);
		return false;
	}
	return true;
}

bool LoadPicture(Picture& pic, const std::string& fpath)
{
	TargaImage tga;

	if (ReadTGAFile(fpath, tga))
	{
		// ReadTGAFile already validated 16-bit depth and the pixel payload.
		pic.m_Width = tga.m_Header.tgaWidth;
		pic.m_Height = tga.m_Header.tgaHeight;

		if (pic.m_Data)
			delete[] pic.m_Data;

		pic.m_Data = new uint16_t[pic.m_Width * pic.m_Height];
		// Menu pictures are 16-bit 5551 (alpha = bit 15). Some source TGAs
		// (e.g. equip_nv.tga) store pixels with bit 15 clear, which the menu
		// treats as fully transparent -> invisible icon. Force the alpha bit
		// so any loaded picture renders opaque like the stock equipment icons.
		const uint16_t* src = reinterpret_cast<const uint16_t*>(tga.m_Data);
		for (unsigned i = 0; i < pic.m_Width * pic.m_Height; ++i) {
			pic.m_Data[i] = static_cast<uint16_t>(src[i] | 0x8000u);
		}

		return true;
	}

	return false;
}


bool LoadText(std::vector<std::string>& txt, const std::string& path)
{
	std::ifstream tf(path);

	if (tf.is_open())
	{
		std::vector<std::string> loaded;
		std::string line;
		while (std::getline(tf, line)) loaded.push_back(line);
		if (tf.bad() || (tf.fail() && !tf.eof())) return false;
		txt.insert(txt.end(), loaded.begin(), loaded.end());
		return true;
	}
	else
	{
		std::cout << "LoadText() : Unable to open text file for reading: '" << path << "'" << std::endl;
	}

	return false;
}


// Load a file path with an optional built-in fallback. The row-specific
// redirect logic lives in LoadRowDescription() above.
void LoadAccessoryDescription(std::vector<std::string>& txt, const std::string& descFile,
                              const std::vector<std::string>* builtinDesc)
{
	if (!LoadText(txt, descFile) && builtinDesc) {
		txt = *builtinDesc;
	}
}


// Built-in flashlight blurb (see the Hunt.h declaration and the addUtil call
// that consumes it). A modernization accessory must not depend on shipped
// data: HUNTDAT comes from the user's own game installation, so most installs
// have no flashlight.nfo and would otherwise show a blank panel plus a
// LoadText() failure on stdout. Kept identical in wording to the file it
// stands in for, and every line under 48 characters because the shop panel
// draws them unwrapped. Defined here rather than in Hunt2.cpp (the usual home
// for EXTERNAL globals) so the menu resource tests can link it.
const std::vector<std::string> kFlashlightDescription = {
	"Accessory : Flashlight",
	"",
	"A torch that casts a warm beam ahead of",
	"you, letting you pick out terrain and game",
	"in the dark without lighting the whole map.",
	"The beam follows your aim and fades with",
	"distance. Press the configured key",
	"(default: F) to switch it on and off. Use",
	"of the flashlight does not affect your",
	"score."
};


bool LoadWave(SoundFX& sfx, const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	std::vector<short> samples;
	const char* error = nullptr;
	if (!file || !c2audio::ReadWave(file, samples, error)) {
		std::cout << "LoadWave(): " << (error ? error : "cannot open WAV file")
		          << ": '" << path << "'" << std::endl;
		return false;
	}
	const auto bytes = samples.size() * sizeof(short);
	std::unique_ptr<int16_t[]> data(new int16_t[samples.size()]);
	memcpy(data.get(), samples.data(), bytes);
	delete[] sfx.m_Data;
	sfx.m_Data = data.release();
	sfx.m_Length = static_cast<uint32_t>(bytes);
	sfx.m_Frequency = c2audio::kSampleRate;
	return true;
}

SoundFX::SoundFX() :
	m_Data(nullptr),
	m_Length(0U),
	m_Frequency(22050U)
{
}


SoundFX::SoundFX(const SoundFX& w) : SoundFX()
{
	if (w.m_Length && w.m_Data) {
		m_Data = new int16_t[(static_cast<size_t>(w.m_Length) + 1) / sizeof(int16_t)];
		memcpy(m_Data, w.m_Data, w.m_Length);
		m_Length = w.m_Length;
	}
	m_Frequency = w.m_Frequency;
}


SoundFX::~SoundFX()
{
	if (m_Data)
		delete[] m_Data;
}


Picture::Picture() :
	m_Width(0),
	m_Height(0),
	m_Data(nullptr)
{}


Picture::Picture(const Picture& p)
{
	m_Data = nullptr;

	if (p.m_Width && p.m_Height && p.m_Data) {
		m_Data = new uint16_t[p.m_Width * p.m_Height];
		memcpy(m_Data, p.m_Data, (p.m_Width * 2) * p.m_Height);
	}

	m_Width = p.m_Width;
	m_Height = p.m_Height;
}


Picture::~Picture()
{
	if (m_Data)
		delete[] m_Data;
}


Picture& Picture::operator= (const Picture& rhs)
{
	m_Data = nullptr;

	if (rhs.m_Width && rhs.m_Height && rhs.m_Data) {
		m_Data = new uint16_t[rhs.m_Width * rhs.m_Height];
		memcpy(m_Data, rhs.m_Data, (rhs.m_Width * 2u) * rhs.m_Height);
	}

	m_Width = rhs.m_Width;
	m_Height = rhs.m_Height;

	return *this;
}

bool Picture::IsValid() const
{
	return (m_Width > 0 && m_Height > 0 && m_Data);
}
