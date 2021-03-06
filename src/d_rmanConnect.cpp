/*
Copyright (c) 2010, Dan Bethell, Johannes Saam.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    * Neither the name of RmanConnect nor the names of its contributors may be
    used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ndspy.h>
#include <iostream>
#include <exception>
#include <cstring>

#include "Client.h"
#include "Data.h"

extern "C"
{
    // open our display driver
    PtDspyError DspyImageOpen(PtDspyImageHandle *pvImage,
            const char *drivername, const char *filename, int width,
            int height, int paramCount, const UserParameter *parameters,
            int formatCount, PtDspyDevFormat *format, PtFlagStuff *flagstuff)
    {
        // get our server hostname from the 'host' or 'hostname' display parameter
        std::string hostname("localhost");
        char *host_tmp = 0;
        DspyFindStringInParamList( "host", &host_tmp, paramCount, parameters );
        DspyFindStringInParamList( "hostname", &host_tmp, paramCount, parameters );
        if ( host_tmp )
            hostname = std::string(host_tmp);

        // get our server port from the 'port' display parameter
        int port_address = 9201;
        DspyFindIntInParamList( "port", &port_address, paramCount, parameters );

        // shuffle format so we always write out RGBA
        std::string chan[4] = { "r", "g", "b", "a" };
        for ( unsigned i=0; i<formatCount; i++ )
            for( unsigned int j=0; j<4; ++j )
                if( std::string(format[i].name)==chan[j] && i!=j )
                {
                    PtDspyDevFormat tmp = format[j];
                    format[j] = format[i];
                    format[i] = tmp;
                }

        // now we can connect to the server and start rendering
        try
        {
            // create a new rmanConnect object
            rmanconnect::Client *client = new rmanconnect::Client( hostname, port_address );

            // make image header & send to server
            rmanconnect::Data header( 0, 0, width, height, formatCount );
            client->openImage( header );

            // create passable pointer for our client object
            *pvImage = reinterpret_cast<PtDspyImageHandle>(client);
        }
        catch (const std::exception &e)
        {
            DspyError("RmanConnect display driver", "%s", e.what());
            DspyError("RmanConnect display driver", "Port '%s:%d'", hostname.c_str(), port_address);
            return PkDspyErrorUndefined;
        }

        return PkDspyErrorNone;
    }

    // handle image data sent from the renderer
    PtDspyError DspyImageData(PtDspyImageHandle pvImage, int xmin,
            int xmax_plusone, int ymin, int ymax_plusone, int entrysize,
            const unsigned char *data)
    {
        try
        {
            rmanconnect::Client *client =
                    reinterpret_cast<rmanconnect::Client*> (pvImage);
            const float *ptr = reinterpret_cast<const float*> (data);

            // create our data object
            rmanconnect::Data data(xmin, ymin, xmax_plusone - xmin,
                    ymax_plusone - ymin, entrysize / sizeof(float), ptr);

            // send it to the server
            client->sendPixels(data);
        }
        catch (const std::exception &e)
        {
            DspyError("RmanConnect display driver", "%s\n", e.what());
            return PkDspyErrorUndefined;
        }
        return PkDspyErrorNone;
    }

    // close the display driver
    PtDspyError DspyImageClose(PtDspyImageHandle pvImage)
    {
        try
        {
            rmanconnect::Client *client =
                    reinterpret_cast<rmanconnect::Client*> (pvImage);
            client->closeImage();
            delete client;
        }
        catch (const std::exception &e)
        {
            DspyError("RmanConnect display driver", "%s\n", e.what());
            return PkDspyErrorUndefined;
        }
        return PkDspyErrorNone;
    }

// some renderer-specific differences
#ifndef DELIGHT_PRE_9_0_45
#define DATALEN_TYPE int
#else
#define DATALEN_TYPE size_t
#endif

    // query the renderer for image details
    PtDspyError DspyImageQuery(PtDspyImageHandle pvImage,
                               PtDspyQueryType querytype, DATALEN_TYPE datalen, void *data)
    {
        if (datalen > 0 && data)
        {
            switch (querytype)
            {
                case PkSizeQuery:
                {
                    PtDspySizeInfo sizeInfo;
                    if (datalen > sizeof(sizeInfo))
                        datalen = sizeof(sizeInfo);
                    sizeInfo.width = 512;
                    sizeInfo.height = 512;
                    sizeInfo.aspectRatio = 1.f;
                    memcpy(data, &sizeInfo, datalen);
                    break;
                }

                case PkOverwriteQuery:
                    // not used in 3delight
                    break;

                default:
                    return PkDspyErrorUnsupported;
            }
        }
        else
        {
            return PkDspyErrorBadParams;
        }
        return PkDspyErrorNone;
    }

} // extern C
