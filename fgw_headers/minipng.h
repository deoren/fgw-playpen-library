// minipng.h -	PNG reading and writing for images with 256 colour palettes.
// 
// By:		Garry Lancaster
// Created:	27 Nov 2002
// Version: 1.0
//
// Notes:
// 1. This PNG implementation only supports 8 bit paletted images and does
//	not support interlaced (a.k.a. progressive display) images. This means
//	that, strictly, it is not compliant with the PNG spec.

#if !defined (MINIPNG_H)
#define MINIPNG_H

#include <stdexcept>
#include <vector>
#include <iosfwd>
#include <string>

namespace studentgraphics {
	// Forward declaration for LoadPlaypen and SavePlaypen functions.
	class playpen;	
}

namespace MiniPNG {

	// Basic image information, including width and height, in pixels.
	class ImageInfo {
	public:
		enum Format {
			// Only one format currently supported.
			Paletted8	// Paletted 8 bits per pixel.
		};

		ImageInfo(unsigned width, unsigned height) :
			width_(width), height_(height) {}

		unsigned GetWidth() const	{return width_;}
		unsigned GetHeight() const	{return height_;}
		Format GetFormat() const	{return Paletted8;}

	private:
		unsigned width_;
		unsigned height_;
	};// class ImageInfo

	// Basic PaletteEntry structure. Could have used HueRGB, but that would
	// be an additional form of coupling to the Playpen.
	struct PaletteEntry {
		unsigned char red;
		unsigned char green;
		unsigned char blue;
	};

	// WritableImage is an abstract class for PNG load operations. It 
	// represents the image into which the contents of the PNG file are
	// loaded.
	// 
	// Exceptions:
	//	Any member function may throw to abort the write (except EndWrite
	//	when its argument is false). Implementations should provide at least 
	//	basic exception safety.
	// Notes:
	// 1. For a single write the order of function calls will be as follows:
	//
	//	- BeginWrite.
	//	- SetImageInfo.
	//	- 256 calls of SetPaletteEntry.
	//	- As many calls of SetScanline as there are rows in the image.
	//	- EndWrite(true).
	//	
	// In the event of an exception, either internally, or thrown from one of
	// the functions themselves, the call sequence will be aborted and the
	// exception propagated. EndWrite(false) will be called during stack
	// unwinding.
	class WritableImage {
	public:
		virtual ~WritableImage(){};
		// Purpose:
		//	Notify that an image write is beginning.
		virtual void BeginWrite() = 0;

		// Purpose:
		//	Set the basic image information for the image.
		// Parameters:
		//	[in] info -	The image information for the image.
		virtual void SetImageInfo(const ImageInfo& info) = 0;

		// Purpose:
		//	Set a palette entry.
		// Parameters:
		//	[in] index -	The zero-based index of the palette entry
		//					being specified. 0 <= entry <= 255.
		//	[in] entry -	The RGB (red, green, blue) value of the palette
		//					entry.
		// Notes:
		// 1. Within a single image write, the index parameter for the first
		//	SetPaletteEntry call is 0 and is incremented for successive calls 
		virtual void SetPaletteEntry(
			unsigned index, const PaletteEntry& entry) = 0;

		// Purpose:
		//	Set the data for a scanline.
		// Parameters:
		//	[in] y -	The zero-based index of the scanline, where 0 is the
		//				topmost scanline.
		//	[in] src -	A pointer to the scanline bytes, the first byte 
		//				belonging to the leftmost pixel, the last byte 
		//				belonging to the rightmost pixel. For Paletted8 format
		//				images there is one byte per pixel and it is a lookup
		//				value into the palette e.g. a value of 10 represents
		//				the RGB value of the palette entry with index 10. This
		//				data is invalid after the call is completed.
		// Notes:
		// 1. Within a single image write, except in case of errors, SetScanline 
		//	is called once for each row in the image, the first call has y == 0
		//	and y is incremented for each successive call.
		virtual void SetScanline(unsigned y, const unsigned char* src) = 0;

		// Purpose:
		//	Notify that a write has ended.
		// Parameters:
		//	[in] success -	If true, indicates that the write was a success,
		//					at least from the loader's point of view.
		//					Otherwise, indicates an exception was thrown.
		// Exceptions:
		//	Must not throw if success == false.
		virtual void EndWrite(bool success) = 0;
	};// class WritableImage

	// ReadableImage is an abstract class for PNG save operations. It
	// represents the image which is being saved.
	//
	// Exceptions:
	//	Any member function may throw to abort the read (except EndRead
	//	when its argument is false). Implementations should provide at least 
	//	basic exception safety.
	// 1. For a single read the order of function calls will be as follows:
	//
	//	- BeginRead.
	//	- GetImageInfo.
	//	- 256 calls of GetPaletteEntry.
	//	- As many calls of GetScanline as there are rows in the image.
	//	- EndRead(true).
	//	
	// In the event of an exception, either internally, or thrown from one of
	// the functions themselves, the call sequence will be aborted and the
	// exception propagated. EndRead(false) will be called during stack
	// unwinding.
	class ReadableImage {
	public:
		virtual ~ReadableImage(){};
		// Purpose:
		//	Notify that an image read is beginning.
		virtual void BeginRead() = 0;

		// Purpose:
		//	Get the basic image information for the image.
		// Returns:
		//	The basic image information, including width and height.
		virtual ImageInfo GetImageInfo() = 0;

		// Purpose:
		//	Get a palette entry from the image palette.
		// Returns:
		//	A palette entry.
		// Parameters:
		//	[in] index -	The zero-based index, from 0 to 255 inclusive, of
		//					the palette entry to be returned.
		// Notes:
		// 1. The first call (within a single image read) has index 0, and 
		//	each successive call increments index.
		virtual PaletteEntry GetPaletteEntry(unsigned index) = 0;

		// Purpose:
		//	Get a scanline's worth of image data.
		// Returns:
		//	A pointer to a scanline worth of image data. The first byte
		//	belongs to the leftmost pixel, the last byte belongs to the 
		//	rightmost pixel. This data must remain valid until the next
		//	call of one of this object's functions.
		// Parameters:
		//	[in] y -	The zero-based y co-ordinate of the scanline being
		//				requested, where 0 is the topmost scanline.
		// Notes:
		// 1. Scanline data must be in the correct format. For Paletted8 this
		//	is width bytes of palette indices.
		// 2. Within a single image read, the first call to GetScanline will
		//	have y == 0 and subsequent calls will increment y.
		virtual const unsigned char* GetScanline(unsigned y) = 0;

		// Purpose:
		//	Notify that an image read has ended.
		// Parameters:
		//	[in] success -	If true, the image read was successful, at least 
		//					as far as the PNG saving system was concerned. If
		//					false, it was not.
		// Notes:
		// 1. Must not throw if success == false.
		virtual void EndRead(bool success) = 0;
	};// class ReadableImage

	// A simple implementation of WritableImage and ReadableImage to use
	// for saving and loading PNG images.
	//
	// The downside of this simplistic approach is memory usage: it stores
	// a complete copy of all the image data (and palette data).
	//
	// Notes:
	// 1. The call order restrictions of the WritableImage and ReadableImage
	//	base classes are removed: functions may be called in any order.
	class SimpleImage : public WritableImage, public ReadableImage
	{
	public:
		SimpleImage(unsigned width, unsigned height);
		virtual ~SimpleImage(){}
		// WritableImage functions.
		virtual void BeginWrite();
		virtual void SetImageInfo(const ImageInfo& info);
		virtual void SetPaletteEntry(
			unsigned index, const PaletteEntry& entry);
		virtual void SetScanline(unsigned y, const unsigned char* src);
		virtual void EndWrite(bool success);

		// ReadableImage functions.
		virtual void BeginRead();
		virtual ImageInfo GetImageInfo();
		virtual PaletteEntry GetPaletteEntry(unsigned index);
		virtual const unsigned char* GetScanline(unsigned y);
		virtual void EndRead(bool success);

	private:
		typedef std::vector<unsigned char> PixelBuffer;

		ImageInfo		info_;
		PaletteEntry	paletteEntries_[256];
		PixelBuffer		pixels_;	
	};// class SimpleImage

	// The free functions may throw this exception to indicate PNG specific
	// errors, as well as the usual memory and I/O related exceptions.
	class error {
	public:
		explicit error(const std::string& m) : m_message( m ) {}
		std::string message() const {return m_message;}

	private:
		std::string m_message;
	};

	// Purpose:
	//	Load a PNG image from a stream.
	// Parameters:
	//	[out] image -	The image to which the loaded data will be written.
	//	[in, out] stm -	The stream from which the PNG data will be read. This
	//					stream must be opened in binary mode.
	// Exceptions:
	//	May throw. Basic guarantee: if it does throw, the stream and image
	//	have valid but indeterminate state.
	void LoadPNG(WritableImage& image, std::istream& stm);

	// Purpose:
	//	Save a PNG image to a stream.
	// Parameters:
	//	[in, out] image -	The image from which the data to be saved will 
	//						be read.
	//	[in, out] stm - The stream to which the image will be saved. The
	//					stream must be opened in binary mode.
	// Exceptions:
	//	May throw. Basic guarantee: if it does throw, the stream and image
	//	have valid but indeterminate state.
	void SavePNG(ReadableImage& image, std::ostream& stm);



}// namespace MiniPNG

namespace fgw {
	using namespace MiniPNG;
}

#endif // Header guards.

