// MiniPNG.cpp
//
// Future Enhancements:
// 1. Support image formats other than 8bpp paletted.
// 2. Support interlacing.
// 3. More sophisticated use of filters during save.
// 4. Support standard non-critical chunks.

#include <climits>	// For UINT_MAX.
#include <stdlib.h>	// Use .h form because MSVC6 has issues with cstdlib.
#include <cstdio>	// For EOF macro.
#include <cassert>
#include <iostream>
#include <vector>
#include <fstream>

#include "minipng.h"
extern "C"{
	#include "zlib.h"		// For (de-)compression.
}
#include "playpen.h"	// For playpen integration.

   namespace {
      using namespace MiniPNG;
   
   // We need a type that can hold at least 32 bits unsigned. unsigned long
   // is always sufficient, according to the C and C++ standards, but on 
   // many platforms we can use an unsigned int, which may be more efficient.
   #define MINIPNG_UINT32_MAX 4294967295
   #if UINT_MAX >= MINIPNG_UINT32_MAX
      typedef unsigned int MiniPNG_UInt32;
   #else
   	typedef unsigned long MiniPNG_UInt32;
   #endif
   
   // This 8 byte signature appears at the start of every PNG file.
      const unsigned PngSignatureByteCount = 8;
      unsigned char PngSignature[PngSignatureByteCount] = 
      {137, 80, 78, 71, 13, 10, 26, 10};
   
   // Standard fixed PNG chunk lengths.
      const MiniPNG_UInt32 IHDRChunkLength = 13;
      const MiniPNG_UInt32 IENDChunkLength = 0;
   
   // IHDR chunk constants.
      const unsigned char		BitDepth		= 8;	// 8 bits per pixel.
      const unsigned char		ColorType		= 3;	// Colour palette.
      const unsigned char		CompressionType	= 0;	// Standard compression.
      const unsigned char		FilterType		= 0;	// Adaptive filtering.
      const unsigned char		InterlaceType	= 0;	// No interlace.
   
   // Prototypes -----------------------------------------------------------
   
   // Wrappers for raw binary stream I/O.
      unsigned char ReadByte(std::istream& stm);
      void WriteByte(std::ostream& stm, unsigned char byte);
      MiniPNG_UInt32 ReadUInt32(std::istream& stm);
      void WriteUInt32(std::ostream& stm, MiniPNG_UInt32 ui);
      void ReadBuffer(std::istream& stm, 
      unsigned char* buf, unsigned len);
      void WriteBuffer(std::ostream& stm, 
      const unsigned char* buf, unsigned len);
   
   // Classes --------------------------------------------------------------
   
   // Abstraction of a 4 byte PNG chunk type code.
       class PNGChunkType {
      public:
          PNGChunkType() : value_(0) {}
          explicit PNGChunkType(MiniPNG_UInt32 type) : value_(type) {}
         explicit PNGChunkType(const char* type);// Type as 4 character string.
      
          bool operator==(const PNGChunkType& rhs) const {
            return value_ == rhs.value_;
         }
      
          bool operator!=(const PNGChunkType& rhs) const {
            return value_ != rhs.value_;
         }
      
          MiniPNG_UInt32 GetValue() const { 
            return value_; }
      
      private:
         MiniPNG_UInt32 value_;
      };// class PNGChunkType
   
   // This class adapted from plain C code in the PNG spec.
       class CRCCalculator {
      public:
         CRCCalculator();
      
      // Purpose:
      //	Append a buffer section to the cyclic redundancy check.
      // Parameters:
      //	[in] buf -	Pointer to the first byte of the buffer section for 
      //				which the CRC should be calculated.
      //	[in] len -	The number of bytes in the buffer.
      // Notes:
      // 1. You can calculate the CRC for a buffer in sections, by calling
      //	Calc multiple times, provided the first buffer byte of each call 
      //	is logically one past the last byte of the previous call.
      // 2. To actually retrieve the CRC, call GetCRC.
         void Append(const unsigned char* buf, unsigned len);
      
      // As above, except appends a single byte.
         void Append(unsigned char val);
      
      // As above, except appends a 4 byte unsigned value in network byte
      // order (most significant byte first).
         void Append(MiniPNG_UInt32 val);
      
      // Returns:
      //	The current cyclic redundancy check.
          MiniPNG_UInt32 GetCRC() const {
            return runningCRC_ ^ 0xFFFFFFFF;}
      
      private:
         enum {TableEntryCount = 256};
      
         static bool				tableInitDone_;
         static MiniPNG_UInt32	table_[TableEntryCount];
      
         MiniPNG_UInt32	runningCRC_;
      };// class CRCCalculator
   
   // Helper class for Compressor and Decompressor.
       class BufferedZLibStream {
      public:
         BufferedZLibStream();
      
         void CompressInit();
         void DecompressInit();
         void CompressUninit();
         void DecompressUninit();
      
         void Compress(const unsigned char* src, unsigned len);
         void CompressFinish();
         void Decompress(const unsigned char* src, unsigned len);
      
          unsigned GetDstLength() const { 
            return writeOffset_; }
          const unsigned char* GetDstPtr() const 	{ 
            return &dstBuffer_[0]; }
          void ClearDst() {writeOffset_ = 0;}
      
      private:
         enum { InitialBufferSize = 1024 };
         typedef std::vector<unsigned char> buffer_type;
      
         buffer_type	dstBuffer_;		// Destination buffer for data.
         unsigned	writeOffset_;	// Offset into destination buffer.
         z_stream	zStm_;			// ZLib (de-)compression stream.
      
         void GrowDstBuffer();
         void PrepareStream(const unsigned char* srcBuffer, unsigned len);
      
      // Prevent copying.
         BufferedZLibStream(const BufferedZLibStream&);
         BufferedZLibStream& operator=(const BufferedZLibStream&);
      };// class BufferedZLibStream
   
   // Uses deflate-type GLib compression.
   // Normal non-error usage is:
   // 1. Construct.
   // 2. Zero or more calls to Compress.
   // 3. Exactly one call to Finish.
   // 4. Destroy.
   // Calls to GetDstLength, GetDstPtr and ClearDst may be made at any point
   // on a fully constructed Compressor, but if you want to get all the output
   // you need to do so after the call to Finish.
       class Compressor {
      public:
          Compressor()	{stm_.CompressInit();}
          ~Compressor()	{stm_.CompressUninit();}
      
          void Compress(const unsigned char* srcBuffer, unsigned len)
         {stm_.Compress(srcBuffer, len);}
      
          void Compress(unsigned char src)	// Compress a single character.
         {stm_.Compress(&src, 1);}
      
          void Finish() {stm_.CompressFinish();}
      
          unsigned GetDstLength() const {
            return stm_.GetDstLength();}
          const unsigned char* GetDstPtr() const 	{
            return stm_.GetDstPtr();}
          void ClearDst() {stm_.ClearDst();}
      
      private:
         BufferedZLibStream stm_;
      };// class Compressor
   
   // Uses inflate-type ZLib decompression.
       class Decompressor {
      public:
          Decompressor()	{stm_.DecompressInit();}
          ~Decompressor() {stm_.DecompressUninit();}
      
          void Decompress(const unsigned char* srcBuffer, unsigned len)
         {stm_.Decompress(srcBuffer, len);}
          const unsigned char* GetDstPtr() const {
            return stm_.GetDstPtr();}
          unsigned GetDstLength() const {
            return stm_.GetDstLength();}
          void ClearDst() {stm_.ClearDst();}
       
      private:
         BufferedZLibStream stm_;
      };// class Decompressor
   
   // Abstract class for filters. Before compression, each scanline is
   // filtered using one of the standard PNG filters and a byte is added
   // to the data to indicate which filter was used. The idea is that 
   // filtering can improve the compression ratio.
       class Filter {
      public:
		virtual ~Filter(){}
      // Purpose:
      //	Notify that a scanline is about to be unfiltered using this filter
      //	algoritm.
      // Parameters:
      //	[in] prior -	Pointer to the first raw (uncompressed, unfiltered)
      //					pixel on the previous scanline,	or 0, if this is 
      //					the topmost scanline.
         virtual void BeginScanline(const unsigned char* prior) = 0;
      
      // Purpose:
      //	Unfilter a byte previously filtered using this algorithm.
      // Returns:
      //	The unfiltered byte value.
      // Parameters:
      //	[in] filtered -	The filtered byte value to be unfiltered.
         virtual unsigned char Unfilter(unsigned char filtered) = 0;
      };// class Filter
   
   // Referred to as Filter Type 0: None in PNG spec.
       class PassThruFilter : public Filter {
      public:
          virtual void BeginScanline(const unsigned char*) {}
          virtual unsigned char Unfilter(unsigned char filtered) 
         { 
            return filtered; }
      };
   
   // Filter Type 1: Sub
       class SubFilter : public Filter {
      public:
         virtual void BeginScanline(const unsigned char* prior);
         virtual unsigned char Unfilter(unsigned char filtered);
      
      private:
         unsigned char leftRaw_;
      };
   
   // Filter Type 2: Up
       class UpFilter : public Filter {
      public:
         virtual void BeginScanline(const unsigned char* prior);
         virtual unsigned char Unfilter(unsigned char filtered);
      
      private:
         const unsigned char*	aboveRaw_;	// NULL pointer if first scanline.
      };
   
   // Filter Type 3: Average
       class AverageFilter : public Filter {
      public:
         virtual void BeginScanline(const unsigned char* prior);
         virtual unsigned char Unfilter(unsigned char filtered);
      
      private:
         const unsigned char*	aboveRaw_;	// NULL pointer if first scanline.
         unsigned char			leftRaw_;
      };// class AverageFilter
   
   // Filter Type 4: Paeth
       class PaethFilter : public Filter {
      public:
         virtual void BeginScanline(const unsigned char* prior);
         virtual unsigned char Unfilter(unsigned char filtered);
      
      private:
         const unsigned char*	aboveRaw_;	// NULL pointer if first scanline.
         const unsigned char*	aboveLeftRaw_;	// NULL if first scanline or x == 0
         unsigned char			leftRaw_;	
      
         static int PaethPredictor(int left, int above, int aboveLeft);	
      };// class PaethFilter
   
       class PNGChunkWriter {
      public:
         PNGChunkWriter(
         std::ostream&		stm, 
         MiniPNG_UInt32		length, 
         const PNGChunkType&	chunkType);
      
         PNGChunkWriter& operator<<(unsigned char byte);
         PNGChunkWriter& operator<<(MiniPNG_UInt32 ui);
         void Write(const unsigned char* buf, unsigned len);		
         void End();
      
      private:
         std::ostream&	stm_;
         MiniPNG_UInt32	length_;
         CRCCalculator	crcCalc_;
      };// class PNGChunkWriter
   
       class PNGChunkReader {
      public:
         explicit PNGChunkReader(std::istream& stm);
      
          MiniPNG_UInt32 GetLength() const { 
            return length_; }
          PNGChunkType GetType() const { 
            return type_; }
          bool IsAncilliary() const 
         { 
            return 0x20000000 == (type_.GetValue() & 0x20000000); }
      
         PNGChunkReader& operator>>(unsigned char& byte);
         PNGChunkReader& operator>>(MiniPNG_UInt32& ui);
         void Read(unsigned char* buf, unsigned len);
         void End();
      
      private:
         std::istream&	stm_;
         MiniPNG_UInt32	length_;
         PNGChunkType	type_;
         CRCCalculator	crcCalc_;
      };// class PNGChunkReader
   
   // Ensures that EndRead is always called to mark the end of an image read,
   // even in the face of exceptions.
       class ReadableImageSentry {
      public:
          explicit ReadableImageSentry(ReadableImage& image) :
          image_(image), success_(false) { 
            image.BeginRead();
         }
          ~ReadableImageSentry()		{image_.EndRead(success_);}
          void EndSuccessfulRead()	{success_ = true;}
      
      private:
         ReadableImage&	image_;
         bool			success_;
      };
   
   // Top-level class for writing a PNG image to a stream.
       class PNGWriter {
      public:
         void operator()(std::ostream& stm, ReadableImage& image);
      
      private:
         std::ostream*			stm_;
         ReadableImage*			image_;
         MiniPNG_UInt32			width_;
         MiniPNG_UInt32			height_;
      
         void WriteSignature();
         void WriteIHDRChunk();
         void WritePLTEChunk();
         void WriteIDATChunks();
         void WriteIENDChunk();
      };// class PNGWriter
   
   // Ensures that EndWrite is always called to mark the end of an image 
   // write, even in the face of exceptions.
       class WritableImageSentry {
      public:
          explicit WritableImageSentry(WritableImage& image) :
          image_(image), success_(false) { 
            image.BeginWrite();
         }
          ~WritableImageSentry()		{image_.EndWrite(success_);}
          void EndSuccessfulWrite()	{success_ = true;}
      
      private:
         WritableImage&	image_;
         bool			success_;
      };
   
   // Top-level class for reading a PNG image from disk.
       class PNGReader {
      public:
         void operator()(std::istream& stm, WritableImage& image);
      
      private:
      // Bitfield for required chunks.
         enum {
         IHDRChunk = 1,
         PLTEChunk = 2,
         IDATChunk = 4,
         IENDChunk = 8,
         AllRequiredChunks = 0xF
         };
      
      // Filter identification codes.
         enum {
         PassThruFilterCode,
         SubFilterCode,
         UpFilterCode,
         AverageFilterCode,
         PaethFilterCode
         };
      
         typedef std::vector<unsigned char>	Buffer;
         typedef Buffer::iterator			BufferIterator;
      
         std::istream*	stm_;
         WritableImage*	image_;
         unsigned		chunksRead_;	// Bitfield of required chunks.
         MiniPNG_UInt32	width_;
         MiniPNG_UInt32	height_;
         Buffer			rawPriorScanline_;	// Buffer for previous raw scanline.	
         Buffer			rawScanline_;	// Buffer for current raw scanline.
         BufferIterator	curRaw_;		// Iterator to current raw pixel.
         BufferIterator	endRaw_;		// Current scanline end iterator.
         int				curY_;			// Current y co-ordinate.
         bool			imageDone_;		// All image data is read.
         Decompressor	decompressor_;
         Buffer			compBuffer_;		// Buffer for compressed data.
      
      // The available filters for doing Unfilter operations.
         PassThruFilter	passThruFilter_;	
         SubFilter		subFilter_;
         UpFilter		upFilter_;
         AverageFilter	averageFilter_;
         PaethFilter		paethFilter_;
      
         Filter*			curFilter_;	// The currently in use filter.
      
         void ReadChunk();
         void CheckSignature();
         void ProcessChunksRead(unsigned curChunk);
         void ReadIHDRChunk(PNGChunkReader& reader);
         void ReadPLTEChunk(PNGChunkReader& reader);
         void ReadIDATChunk(PNGChunkReader& reader);
         void ReadIENDChunk(PNGChunkReader& reader);
         void ReadUnknownChunk(PNGChunkReader& reader);
      };// class PNGReader
   
   // Standard PNG chunk type codes.
      const PNGChunkType IHDRChunkType = PNGChunkType("IHDR");
      const PNGChunkType PLTEChunkType = PNGChunkType("PLTE");
      const PNGChunkType IDATChunkType = PNGChunkType("IDAT");
      const PNGChunkType IENDChunkType = PNGChunkType("IEND");
   
   // Free functions -------------------------------------------------------
   
       unsigned char ReadByte(std::istream& stm) {
         char c;
      
         if (!stm.get(c)) {
            throw error("Bad stream in ReadByte.");
         }
      
         return *static_cast<unsigned char*>(static_cast<void*>(&c));
      }
   
       void WriteByte(std::ostream& stm, unsigned char byte) {
         if (!stm) {
            throw error("Bad stream in WriteByte.");
         }
      
         char c = *(static_cast<char*>(static_cast<void*>(&byte)));
         stm.put(c);
      }
   
       MiniPNG_UInt32 ReadUInt32(std::istream& stm) {
         MiniPNG_UInt32 ui;
      
      // PNG uses network byte order i.e. most significant byte first.
         ui = static_cast<MiniPNG_UInt32>(ReadByte(stm)) << 24;
         ui |= static_cast<MiniPNG_UInt32>(ReadByte(stm)) << 16;
         ui |= static_cast<MiniPNG_UInt32>(ReadByte(stm)) << 8;
         ui |= static_cast<MiniPNG_UInt32>(ReadByte(stm));
      
         return ui;
      }
   
       void WriteUInt32(std::ostream& stm, MiniPNG_UInt32 ui) {
      // PNG uses network byte order i.e. most significant byte first.
         WriteByte(stm, (ui & 0xFF000000) >> 24);
         WriteByte(stm, (ui & 0x00FF0000) >> 16);
         WriteByte(stm, (ui & 0x0000FF00) >> 8);
         WriteByte(stm, ui & 0x000000FF);
      }
   
       void WriteBuffer(std::ostream& stm, 
       const unsigned char* buf, unsigned len) {
      
         for (unsigned i = 0; i < len; ++i) {
            WriteByte(stm, *buf++);
         }
      }
   
       void ReadBuffer(std::istream& stm, 
       unsigned char* buf, unsigned len) {
      
         for (unsigned i = 0; i < len; ++i) {
            *buf++ = ReadByte(stm);
         }
      }
   
   // PNGChunkType ---------------------------------------------------------
   
       PNGChunkType::PNGChunkType(const char* type) {
         value_ = static_cast<MiniPNG_UInt32>(type[0]) << 24;
         value_ |= static_cast<MiniPNG_UInt32>(type[1]) << 16;
         value_ |= static_cast<MiniPNG_UInt32>(type[2]) << 8;
         value_ |= static_cast<MiniPNG_UInt32>(type[3]);		
      }
   
   // SubFilter ------------------------------------------------------------
   
   /*virtual*/ 
       void SubFilter::BeginScanline(const unsigned char*) {
         leftRaw_ = 0;
      }
   
   /*virtual*/ 
       unsigned char SubFilter::Unfilter(unsigned char filtered) {
      // The PNG spec says:
      //	Sub(x) = Raw(x) - Raw(x-bpp)
      // So,
      //	Raw(x) = Sub(x) + Raw(x-bpp)
         unsigned char raw = 
            (static_cast<unsigned>(filtered) + leftRaw_) & 0xFF;
         leftRaw_ = raw;
         return raw;
      }
   
   // UpFilter -------------------------------------------------------------
   
   /*virtual*/ 
       void UpFilter::BeginScanline(const unsigned char* prior) {
         aboveRaw_ = prior;
      }
   
   /*virtual*/ 
       unsigned char UpFilter::Unfilter(unsigned char filtered) {
      // PNG Spec says:
      //	Up(x) = Raw(x) - Prior(x)
      // So,
      //	Raw(x) = Up(x) + Prior(x)
         unsigned char raw = filtered;
         if (aboveRaw_) {
            raw = (static_cast<unsigned>(raw) + *aboveRaw_++) & 0xFF;
         }
         return raw;
      }
   
   // AverageFilter --------------------------------------------------------
   
   /*virtual*/ 
       void AverageFilter::BeginScanline(const unsigned char* prior) {
         aboveRaw_	= prior;
         leftRaw_	= 0;
      }
   
   /*virtual*/ 
       unsigned char AverageFilter::Unfilter(
       unsigned char filtered) {
      // PNG spec says:
      //	Average(x) = Raw(x) - floor((Raw(x-bpp)+Prior(x))/2)
      // So,
      //	Raw(x) = Average(x) + floor((Raw(x-bpp)+Prior(x))/2)
         unsigned char prior	= 0;		
         if (aboveRaw_) {
            prior = *aboveRaw_++;
         }
         unsigned char raw = 0xFF & (filtered + 
            ((static_cast<unsigned>(leftRaw_) + prior) >> 1));
         leftRaw_ = raw;
         return raw;
      }// AverageFilter::Unfilter
   
   // PaethFilter ----------------------------------------------------------
   
   /*virtual*/ 
       void PaethFilter::BeginScanline(const unsigned char* prior) {
         leftRaw_		= 0;
         aboveRaw_		= prior;
         aboveLeftRaw_	= 0;
      }
   
   /*virtual*/ 
       unsigned char PaethFilter::Unfilter(unsigned char filtered) {
      // PNG spec says:
      //	Paeth(x) = Raw(x) - 
      //		PaethPredictor(Raw(x-bpp), Prior(x), Prior(x-bpp))
      // So,
      //	Raw(x) = Paeth(x) + PaethPredictor(...)
         unsigned char aboveLeft	= 0;
         if (aboveLeftRaw_) {
            aboveLeft = *aboveLeftRaw_;
         }
         unsigned char above	= 0;
         if (aboveRaw_) {
            above = *aboveRaw_;
            aboveLeftRaw_ = aboveRaw_++;
         }
         unsigned char raw = 0xFF & (static_cast<int>(filtered) +
            PaethPredictor(leftRaw_, above, aboveLeft));
         leftRaw_		= raw;
         return raw;
      }// PaethFilter::Unfilter
   
   /*static*/ 
       int PaethFilter::PaethPredictor(
       int left, int above, int aboveLeft) {
      // Translated from pseudocode in PNG spec.
         int initialEstimate		= left + above - aboveLeft;
         int leftDistance		= abs(initialEstimate - left);
         int aboveDistance		= abs(initialEstimate - above);
         int aboveLeftDistance	= abs(initialEstimate - aboveLeft);
      
      // Return nearest of left, above or aboveLeft, breaking ties in order
      // left, above, aboveLeft.
         if (leftDistance <= aboveDistance && 
         leftDistance <= aboveLeftDistance) {
            return left;
         } 
         else if (aboveDistance <= aboveLeftDistance) {
            return above;
         } 
         else {
            return aboveLeft;
         }
      }// PaethFilter::PaethPredictor
   
   // BufferedZLibStream ---------------------------------------------------
   
       BufferedZLibStream::BufferedZLibStream() : 
       dstBuffer_	(InitialBufferSize),
       writeOffset_(0) {
      
         zStm_.zalloc	= (alloc_func)0;
         zStm_.zfree		= (free_func)0;
         zStm_.opaque	= (voidpf)0;
      }// BufferedZLibStream ctor
   
       void BufferedZLibStream::CompressInit() {
         int err = deflateInit(&zStm_, Z_DEFAULT_COMPRESSION);
         if (Z_OK != err) {
            throw error(
               "deflateInit failed in BufferdZLibStream::CompressInit.");
         }
      }
   
       void BufferedZLibStream::DecompressInit() {
         int err = inflateInit(&zStm_);
         if (Z_OK != err) {
            throw error(
               "inflateInit failed in BufferedZLibStream::DecompressInit.");
         }
      }
   
       void BufferedZLibStream::CompressUninit() {
         deflateEnd(&zStm_);
      }
   
       void BufferedZLibStream::DecompressUninit() {
         inflateEnd(&zStm_);	
      }
   
       void BufferedZLibStream::Compress(
       const unsigned char* src, unsigned len) {
      
         PrepareStream(src, len);
      
      // While there is still uncompressed source, keep compressing.
         while (zStm_.avail_in != 0) {
            if (0 == zStm_.avail_out) {
               GrowDstBuffer();
            }
            uLong oldTotalOut = zStm_.total_out;
            int err = deflate(&zStm_, Z_NO_FLUSH);
            writeOffset_ += zStm_.total_out - oldTotalOut;
            if (Z_OK != err) {
               throw error(
                  "deflate failed in BufferedZLibStream::Compress.");
            }
         }
      }// BufferedZLibStream::Compress
   
       void BufferedZLibStream::CompressFinish() {
         PrepareStream(0, 0);
      
         bool moreOutput = true;
         do {
            uLong oldTotalOut = zStm_.total_out;
            int err = deflate(&zStm_, Z_FINISH);
            writeOffset_ += zStm_.total_out - oldTotalOut;
            if (err == Z_STREAM_END) {
               moreOutput = false;
            } 
            else if (err == Z_OK) {
               GrowDstBuffer();
            } 
            else {
               throw error(
                  "deflate failed in BufferedZLibStream::CompressFinish.");
            }
         } while (moreOutput);		
      }// BufferedZLibStream::CompressFinish
   
       void BufferedZLibStream::Decompress(
       const unsigned char* src, unsigned len) {
      
         PrepareStream(src, len);
      
         while (zStm_.avail_in != 0) {
            if (0 == zStm_.avail_out) {
               GrowDstBuffer();
            }
            uLong oldTotalOut = zStm_.total_out;
            int err = inflate(&zStm_, Z_NO_FLUSH);
            writeOffset_ += zStm_.total_out - oldTotalOut;
            if (err == Z_STREAM_END) 
               break;
            if (err != Z_OK) {
               throw error(
                  "inflate failed in BufferedZLibStream::Decompress.");
            }
         }		
      }// BufferedZLibStream::Decompress
   
       void BufferedZLibStream::GrowDstBuffer() {
         unsigned oldLength = dstBuffer_.size();
         dstBuffer_.resize(oldLength * 2);
         zStm_.avail_out	+= oldLength;
         zStm_.next_out	= &dstBuffer_[writeOffset_];
      }
   
       void BufferedZLibStream::PrepareStream(
       const unsigned char* srcBuffer, unsigned len) {
      
         zStm_.next_in	= (Bytef*) srcBuffer;
         zStm_.avail_in	= len;
         zStm_.next_out	= &dstBuffer_[writeOffset_];
         zStm_.avail_out	= dstBuffer_.size() - writeOffset_;
      }// BufferedZLibStream::PrepareStream
   
   // PNGChunkWriter -------------------------------------------------------
   
       PNGChunkWriter::PNGChunkWriter(
       std::ostream&		stm, 
       MiniPNG_UInt32		length, 
       const PNGChunkType&	chunkType) :
       stm_	(stm),
       length_(length) {
      
         WriteUInt32(stm, length);
         WriteUInt32(stm, chunkType.GetValue());
         crcCalc_.Append(chunkType.GetValue());
      }
   
       PNGChunkWriter& PNGChunkWriter::operator<<(unsigned char byte) {
         WriteByte(stm_, byte);
         crcCalc_.Append(byte);
         return *this;
      }
   
       PNGChunkWriter& PNGChunkWriter::operator<<(MiniPNG_UInt32 ui) {
         WriteUInt32(stm_, ui);
         crcCalc_.Append(ui);
         return *this;
      }
   
       void PNGChunkWriter::Write(const unsigned char* buf, unsigned len) {
         WriteBuffer(stm_, buf, len);
         crcCalc_.Append(buf, len);
      }
   
       void PNGChunkWriter::End() {
         WriteUInt32(stm_, crcCalc_.GetCRC());
      }
   
   // PNGChunkReader -------------------------------------------------------
   
       PNGChunkReader::PNGChunkReader(std::istream& stm) :
       stm_(stm) {
         length_	= ReadUInt32(stm_);
         type_	= PNGChunkType(ReadUInt32(stm_));
         crcCalc_.Append(type_.GetValue());
      }
   
       PNGChunkReader& PNGChunkReader::operator>>(unsigned char& byte) {
         byte = ReadByte(stm_);
         crcCalc_.Append(byte);
         return *this;
      }
   
       PNGChunkReader& PNGChunkReader::operator>>(MiniPNG_UInt32& ui) {
         ui = ReadUInt32(stm_);
         crcCalc_.Append(ui);
         return *this;
      }
   
       void PNGChunkReader::Read(unsigned char* buf, unsigned len) {
         ReadBuffer(stm_, buf, len);
         crcCalc_.Append(buf, len);	
      }
   
       void PNGChunkReader::End() {
         MiniPNG_UInt32 fileCrc = ReadUInt32(stm_);
      
         if (fileCrc != crcCalc_.GetCRC()) {
            throw error("PNGChunkReader::end found bad CRC.");
         }
      }
   
   // PNGWriter ------------------------------------------------------------
   
       void PNGWriter::operator()(std::ostream& stm, ReadableImage& image) {
         stm_	= &stm;
         image_	= &image;
      
         ReadableImageSentry sentry(image);
      
         ImageInfo info = image.GetImageInfo();
         width_	= info.GetWidth();
         height_	= info.GetHeight();
         if (!width_) {
            throw error("PNGWriter found illegal (zero) width value.");
         }
         if (!height_) {
            throw error("PNGWriter found illegal (zero) height value.");
         }
         assert(ImageInfo::Paletted8 == info.GetFormat());
      
         WriteSignature();
         WriteIHDRChunk();
         WritePLTEChunk();
         WriteIDATChunks();
         WriteIENDChunk();		 
      
         sentry.EndSuccessfulRead();
      }// PNGWriter::operator()
   
       void PNGWriter::WriteSignature() {
         for (unsigned i = 0; i < PngSignatureByteCount; ++i) {
            WriteByte(*stm_, PngSignature[i]);
         }
      }// PNGWriter::WriteSignature
   
       void PNGWriter::WriteIHDRChunk() {
         PNGChunkWriter writer(*stm_, IHDRChunkLength, IHDRChunkType);
      
         writer << width_ << height_ << BitDepth << ColorType << 
            CompressionType << FilterType << InterlaceType;
         writer.End();
      }
   
       void PNGWriter::WritePLTEChunk() {
         const MiniPNG_UInt32 chunkLength = 3 * 256;
      
         PNGChunkWriter writer(*stm_, chunkLength, PLTEChunkType);
      
         for (unsigned i = 0; i < 256; ++i) {
            PaletteEntry entry = image_->GetPaletteEntry(i);
            writer << entry.red;
            writer << entry.green;
            writer << entry.blue;
         }
         writer.End();
      }// PNGWriter::WritePLTEChunk
   
       void PNGWriter::WriteIDATChunks() {
         Compressor compressor;
      
      // Write IDAT chunks. One per scanline except for empty chunks.
         for (unsigned y = 0; y < height_; ++y) {
         // Compress scanline with filter style 0: none (PassThruFilter).
            const unsigned char* curPixel = image_->GetScanline(y);
            compressor.Compress(static_cast<unsigned char>(0));	
            compressor.Compress(curPixel, width_);
         
            if (y == height_ - 1) {
            // Final scanline: output compression stream postscript.
               compressor.Finish();
            }
         
            if (compressor.GetDstLength()) {
            // Write compressed scanline to an IDAT chunk.
               PNGChunkWriter writer(
                  *stm_, compressor.GetDstLength(), IDATChunkType);
            
               writer.Write(compressor.GetDstPtr(), compressor.GetDstLength());
               compressor.ClearDst();
               writer.End();
            }
         }// for( y...
      }// PNGWriter::WriteIDATChunks
   
       void PNGWriter::WriteIENDChunk() {
         PNGChunkWriter writer(*stm_, IENDChunkLength, IENDChunkType);
         writer.End();
      }
   
   // PNGReader ------------------------------------------------------------
   
       void PNGReader::operator()(std::istream& stm, WritableImage& image) {
         stm_		= &stm;
         image_		= &image;
         chunksRead_	= 0;
         curY_		= -1;
         imageDone_	= false;
      
         WritableImageSentry sentry(image);
      
         CheckSignature();
         while (EOF != stm_->peek()) {
            ReadChunk();
         }
      
         if (chunksRead_ != AllRequiredChunks) {
            throw error("PNGReader: missing required chunk.");
         }
      
         if (!imageDone_) {
            throw error("PNGReader: incomplete image data.");
         }
      
         sentry.EndSuccessfulWrite();
      }// PNGReader::operator()
   
       void PNGReader::CheckSignature() {
         for (unsigned i = 0; i < PngSignatureByteCount; ++i) {
            unsigned char byte = ReadByte(*stm_);
         
            if (byte != PngSignature[i]) {
               throw error(
                  "Unexpected signature value in PNGReader::CheckSignature.");
            }
         }
      }// PNGReader::CheckSignature
   
       void PNGReader::ReadChunk() {
         PNGChunkReader	reader(*stm_);
         PNGChunkType	type(reader.GetType());
      
         if (chunksRead_ & IENDChunk) {
         // IEND chunk must be the last chunk.
            throw error(
               "PngChunk::ReadChunk found chunk after IEND chunk.");
         }
      
         if (type == IHDRChunkType) {
            ProcessChunksRead(IHDRChunk);
            ReadIHDRChunk(reader);
         } 
         else if (type == PLTEChunkType) {
            ProcessChunksRead(PLTEChunk);
            ReadPLTEChunk(reader);
         } 
         else if (type == IDATChunkType) {
            ProcessChunksRead(IDATChunk);
            ReadIDATChunk(reader);
         } 
         else if (type == IENDChunkType) {
            ProcessChunksRead(IENDChunk);
            ReadIENDChunk(reader);
         } 
         else {
            ReadUnknownChunk(reader);
         }
      
         reader.End();
      }// PNGReader::ReadChunk
   
       void PNGReader::ProcessChunksRead(unsigned curChunk) {
         if (curChunk != IDATChunk && chunksRead_ & curChunk) {
         // N.B. IDAT is the only required chunk that is permitted to
         // occur multiple times.
            throw error(
               "PNGReader::ProcessChunksRead found duplicate chunk.");
         }
      
         if ((curChunk << 1) <= chunksRead_) {
         // We have already seen a chunk that should come later.
            throw error(
               "PNGReader::ProcessChunksRead found out-of-order chunk.");
         }
      
         chunksRead_ |= curChunk;
      
         if (!(chunksRead_ & IHDRChunk)) {		
            throw error("PNGReader::ProcessChunksRead found a chunk before "
               "the IHDR chunk.");
         }
      }// PNGReader::ProcessChunksRead
   
       void PNGReader::ReadIHDRChunk(PNGChunkReader& reader) {
         reader >> width_ >> height_;
      
         if (!width_) {
            throw error("PNGReader::ReadIHDRChunk got 0 width.");
         }
         if (!height_) {
            throw error("PNGReader::ReadIHDRChunk got 0 height.");
         }
      
         unsigned char	bitDepth;
         unsigned char	colorType;
         unsigned char	compressionType;
         unsigned char	filterType;
         unsigned char	interlaceType;
      
         reader >> bitDepth >> colorType >> 
            compressionType >> filterType >> interlaceType;
      
         if (bitDepth != BitDepth) {
            throw error("ReadIHDRChunk detected unsupported bit depth.");
         }
         if (colorType != ColorType) {
            throw error("ReadIHDRChunk detected unsupported color type.");
         }
         if (compressionType != CompressionType) {
            throw error(
               "ReadIHDRChunk detected unsupported compression type.");
         }
         if (filterType != FilterType) {
            throw error("ReadIHDRChunk detected unsupported filter type.");
         }
         if (interlaceType != InterlaceType) {
            throw error(
               "ReadIHDRChunk detected unsupported interlace type.");
         }
      
         image_->SetImageInfo(ImageInfo(width_, height_));
      
      // Initialise members for first image data read.
         rawScanline_.resize(width_);
         rawPriorScanline_.resize(width_);
         endRaw_ = curRaw_ = rawScanline_.begin();
      }// PNGReader::ReadIHDRChunk
   
       void PNGReader::ReadPLTEChunk(PNGChunkReader& reader) {
         unsigned entryCount = reader.GetLength() / 3;
      
         if (reader.GetLength() % 3) {
         // PLTE length must be a multiple of 3.
            throw error(
               "PNGReader::ReadPLTEChunk detected bad PLTE chunk length.");
         }
      
         for (unsigned i = 0; i < entryCount; ++i) {
            PaletteEntry entry;
            reader >> entry.red >> entry.green >> entry.blue;
            image_->SetPaletteEntry(i, entry);
         }
      }// PNGReader::ReadPLTEChunk
   
       void PNGReader::ReadIDATChunk(PNGChunkReader& reader) {
         assert(rawScanline_.size() == width_);
      
      // Decompress chunk.
         MiniPNG_UInt32 compLength = reader.GetLength();
         compBuffer_.resize(compLength);
         reader.Read(&compBuffer_[0], compLength);
         decompressor_.Decompress(&compBuffer_[0], compLength);
      
      // Unfilter decompressed chunk data, including interpreting start-of-
      // scanline filter code bytes.
         const unsigned char*	cur = decompressor_.GetDstPtr();
         const unsigned char*	end = cur + decompressor_.GetDstLength();
         while (cur != end) {
            if (curRaw_ == endRaw_) {
            // Reached start of next scanline.
               if (++curY_ == (int)height_) {
               // Prevent buffer overrun.
                  throw error(
                     "PNGWriter::ReadIDATChunk found too many pixels.");
               }
            
               rawScanline_.swap(rawPriorScanline_);
               curRaw_ = rawScanline_.begin();
               endRaw_	= rawScanline_.end();
            
            // Read filter byte.
               switch (*cur) {
                  case PassThruFilterCode:
                     curFilter_ = &passThruFilter_;
                     break;
               
                  case SubFilterCode:
                     curFilter_ = &subFilter_;
                     break;
               
                  case UpFilterCode:
                     curFilter_ = &upFilter_;
                     break;
               
                  case AverageFilterCode:
                     curFilter_ = &averageFilter_;
                     break;
               
                  case PaethFilterCode:
                     curFilter_ = &paethFilter_;
                     break;
               }// switch (filter code)
            
               const unsigned char* prior = 0;
               if (curY_) {
                  prior = &rawPriorScanline_[0];
                  image_->SetScanline(curY_ - 1, prior);
               }
               curFilter_->BeginScanline(prior);
            } 
            else {
            // Normal pixel byte.
               *curRaw_++ = curFilter_->Unfilter(*cur);
            }
            ++cur;
         }// while (cur != end)
      
         if (curRaw_ == endRaw_ && (MiniPNG_UInt32)curY_ == (height_ - 1)) {
         // Write final scanline.	
            image_->SetScanline(curY_, &rawScanline_[0]);
            imageDone_ = true;
         }
      
         decompressor_.ClearDst();
      }// ReadIDATChunk
   
       void PNGReader::ReadIENDChunk(PNGChunkReader& reader) {
      // IEND chunks are empty.
      }
   
       void PNGReader::ReadUnknownChunk(PNGChunkReader& reader) {
         if (!reader.IsAncilliary()) {
         // Non-ancilliary chunks must be processed for a successful image
         // read. If we come across one we don't understand we should not
         // continue.
            throw error(
               "PNGReader::ReadUnknownChunk detected non-ancilliary chunk.");
         }
      
      // Skip past unknown chunk.
         unsigned char dummy;
         unsigned length = reader.GetLength();
         while (length--) {
            reader >> dummy;
         }
      }// PNGReader::ReadUnknownChunk
   
   // CRCCalculator --------------------------------------------------------
   
   /*static*/ bool	CRCCalculator::tableInitDone_ = false;
   /*static*/ MiniPNG_UInt32	
      CRCCalculator::table_[CRCCalculator::TableEntryCount];
   
       CRCCalculator::CRCCalculator() : runningCRC_(0xFFFFFFFF) {
         if (!tableInitDone_) {
         // Initialise CRC lookup table.
            for (unsigned n = 0; n < TableEntryCount; ++n) {
               MiniPNG_UInt32 c = n;
               for (unsigned k = 0; k < 8; ++k) {
                  if (c & 1) {
                     c = 0xEDB88320 ^ (c >> 1);
                  } 
                  else {
                     c >>= 1;
                  }
               }// for (k...
               table_[n] = c;
            }// for (n...
            tableInitDone_ = true;
         }// if (!tableInitDone_)
      }// CRCCalculator ctor.
   
       void CRCCalculator::Append(const unsigned char* buf, unsigned len) {
      
         MiniPNG_UInt32 c = runningCRC_;
      
         const unsigned char* lim = buf + len;
         while (buf != lim) {
            c = table_[(c ^ *buf++) & 0xFF] ^ (c >> 8);
         }
      
         runningCRC_ = c;
      }// CRCCalculator::Append
   
       void CRCCalculator::Append(MiniPNG_UInt32 val) {
         unsigned char buf[4];
      
      // Create a buffer containing val in network byte order (i.e. most
      // significant byte first).
         buf[0] = (val & 0xFF000000) >> 24;
         buf[1] = (val & 0x00FF0000) >> 16;
         buf[2] = (val & 0x0000FF00) >> 8;
         buf[3] = val & 0x000000FF;
      
         CRCCalculator::Append(buf, 4);
      }
   
       void CRCCalculator::Append(unsigned char val) {
         Append(&val, 1);
      }
   
   }// anonymous namespace

   namespace MiniPNG {
   
   // SimpleImage ----------------------------------------------------------
   
       SimpleImage::SimpleImage(unsigned width, unsigned height) :
       info_	(width, height),
       pixels_	(width * height)
      {}
   
   /*virtual*/ 
       void SimpleImage::BeginWrite() {}
   
   /*virtual*/ 
       void SimpleImage::SetImageInfo(const ImageInfo& info) {
         assert(ImageInfo::Paletted8 == info.GetFormat());
      
         unsigned newBufSize = info.GetWidth() * info.GetHeight();
         if (newBufSize < pixels_.size()) {
         // Shrink vector.
            PixelBuffer newBuf(newBufSize);
            newBuf.swap(pixels_);
         } 
         else {
         // Grow vector.
            pixels_.resize(newBufSize);
         }
      
         info_ = info;
      }
   
   /*virtual*/ 
       void SimpleImage::SetPaletteEntry(
       unsigned index, const PaletteEntry& entry) {
      
         assert(index <= 255);
         paletteEntries_[index] = entry;
      }
   
   /*virtual*/ 
       void SimpleImage::SetScanline(
       unsigned y, const unsigned char* src) {
      
         assert(y < info_.GetHeight());
         assert(src);
      
         PixelBuffer::iterator cur = pixels_.begin() + (y * info_.GetWidth());
         PixelBuffer::iterator end = cur + info_.GetWidth();
         while (cur != end) {
            *cur++ = *src++;
         }
      }
   
   /*virtual*/ 
       void SimpleImage::EndWrite(bool success) {}
   
   /*virtual*/ 
       void SimpleImage::BeginRead() {}
   
   /*virtual*/ 
       ImageInfo SimpleImage::GetImageInfo() {
         return info_;
      }
   
   /*virtual*/ 
       PaletteEntry SimpleImage::GetPaletteEntry(unsigned index) {
         assert(index <= 255);
         return paletteEntries_[index];
      }
   
   /*virtual*/ 
       const unsigned char* SimpleImage::GetScanline(unsigned y) {
         assert(y < info_.GetHeight());
         return &pixels_[info_.GetWidth() * y];
      }
   
   /*virtual*/ 
       void SimpleImage::EndRead(bool success) {}
   
   // Free functions -------------------------------------------------------
   
       void LoadPNG(WritableImage& image, std::istream& stm) {
         PNGReader reader;
         reader(stm, image);
      }
   
       void SavePNG(ReadableImage& image, std::ostream& stm) {
         PNGWriter writer;
         writer(stm, image);
      }
   
       void LoadPlaypen(studentgraphics::playpen& p, std::istream& stm) {
         using namespace studentgraphics;
      
         SimpleImage image(0, 0);
         LoadPNG(image, stm);
      
         ImageInfo info = image.GetImageInfo();
         if (info.GetWidth() != (unsigned)Xpixels || info.GetHeight() != (unsigned)Ypixels) {
            throw playpen::exception(playpen::exception::error,
               "LoadPlaypen found loaded image was wrong size.");
         }
      
         for (unsigned i = 0; i < colours; ++i) {
            PaletteEntry entry = image.GetPaletteEntry(i);
            p.setpalettentry(int(i), HueRGB(entry.red, entry.green, entry.blue));
         }
      
         for (int y = 0; y < Ypixels; ++y) {
            const unsigned char* cur = image.GetScanline(y);
            for (int x = 0; x < Xpixels; ++x) {
               p.setrawpixel(x, y, *cur++);
            }
         }
      
         p.updatepalette();
         p.display();
      }// LoadPlaypen
   
   
       void SavePlaypen(studentgraphics::playpen const & p, std::ostream& stm) {
         using namespace studentgraphics;
      
         SimpleImage image(Xpixels, Ypixels);
      
         for (unsigned i = 0; i < colours; ++i) {
            HueRGB			hueRGB	= p.getpalettentry(int(i));
            PaletteEntry	entry	= {hueRGB.r, hueRGB.g, hueRGB.b};
            image.SetPaletteEntry(i, entry);
         }
      
         std::vector<palettecode> scanline(Xpixels);
         for (int y = 0; y < Ypixels; ++y) {
            std::vector<palettecode>::iterator cur = scanline.begin();
            for (int x = 0; x < Xpixels; ++x) {
               *cur++ = p.getrawpixel(x, y);
            }
            image.SetScanline(y, &scanline[0]);
         }
      
         SavePNG(image, stm);
      }// SavePlaypen
   
   }// namespace MiniPNG
	// overload for public use to prevent problems with not opening stream in binary mode
   namespace studentgraphics {
       void LoadPlaypen(playpen & p, std::string filename) {
         std::ifstream infile(filename.c_str(), std::ios::binary);
         if(!infile)throw MiniPNG::error("Cannot provide access to input file in LoadPlaypen");
         MiniPNG::LoadPlaypen(p, infile);
         infile.close();
      }	 		   
   
   // overload for public use to prevent problems with not opening stream in binary mode
   
       void SavePlaypen(playpen const & p, std::string filename) {
         std::ofstream outfile(filename.c_str(), std::ios::binary);
         if(!outfile)throw MiniPNG::error("Cannot provide access to output file in SavePlaypen");
         MiniPNG::SavePlaypen(p, outfile);
         outfile.close();
      }	 	 	 	 
   } // end namespace studentgraphics
