/*
 Copyright (C) Giuliano Catrambone (giuliano.catrambone@catrasoftware.it)

 This program is free software; you can redistribute it and/or 
 modify it under the terms of the GNU General Public License 
 as published by the Free Software Foundation; either 
 version 2 of the License, or (at your option) any later 
 version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 Commercial use other than under the terms of the GNU General Public
 License is allowed only after express negotiation of conditions
 with the authors.
*/

#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include "Compressor.h"


Compressor:: Compressor (void)

{

}


Compressor:: ~Compressor (void)

{

}


Compressor:: Compressor (const Compressor &)

{

	assert (1==0);

	// to do
}


Compressor &Compressor:: operator = (const Compressor &)

{

	assert (1==0);

	// to do

	return *this;
}

/** Compress a STL string using zlib with given compression level and return
  * the binary data. */
// linux command using this compresion: zlib-flate
//		i.e.: cat a.zip | zlib-flate -uncompress
//			where a.zip is the output of the CatraMMS API
// Gzip is deflate plus a few headers and a check sum
// https://dev.to/biellls/compression-clearing-the-confusion-on-zip-gzip-zlib-and-deflate-15g1
string Compressor::compress_string(const string& toBeCompressed, int compressionlevel)
{
    z_stream zStream;                        // z_stream is zlib's control structure
    memset(&zStream, 0, sizeof(zStream));

    if (deflateInit(&zStream, compressionlevel) != Z_OK)
        throw(runtime_error("deflateInit failed while compressing."));

    zStream.next_in = (Bytef*) toBeCompressed.data();
    zStream.avail_in = toBeCompressed.size();           // set the z_stream's input

    int ret;
    char outBuffer[32768];
    string outString;

    // retrieve the compressed bytes blockwise
    do {
        zStream.next_out = reinterpret_cast<Bytef*>(outBuffer);
        zStream.avail_out = sizeof(outBuffer);

        ret = deflate(&zStream, Z_FINISH);

        if (outString.size() < zStream.total_out) {
            // append the block to the output string
            outString.append(outBuffer, zStream.total_out - outString.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zStream);

    if (ret != Z_STREAM_END)
	{          // an error occurred that was not EOF
        ostringstream oss;
        oss << "Exception during zlib compression: (" << ret << ") " << zStream.msg;
        throw(runtime_error(oss.str()));
    }

    return outString;
}

/** Decompress an STL string using zlib and return the original data. */
string Compressor::decompress_string(const string& compressed)
{
    z_stream zStream;                        // z_stream is zlib's control structure
    memset(&zStream, 0, sizeof(zStream));

    if (inflateInit(&zStream) != Z_OK)
        throw(runtime_error("inflateInit failed while decompressing."));

    zStream.next_in = (Bytef*)compressed.data();
    zStream.avail_in = compressed.size();

    int ret;
    char outBuffer[32768];
    string outString;

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zStream.next_out = reinterpret_cast<Bytef*>(outBuffer);
        zStream.avail_out = sizeof(outBuffer);

        ret = inflate(&zStream, 0);

        if (outString.size() < zStream.total_out) {
            outString.append(outBuffer,
                             zStream.total_out - outString.size());
        }

    } while (ret == Z_OK);

    inflateEnd(&zStream);

    if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
        ostringstream oss;
        oss << "Exception during zlib decompression: (" << ret << ") "
            << zStream.msg;
        throw(runtime_error(oss.str()));
    }

    return outString;
}

