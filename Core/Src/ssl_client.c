/*
 *  Copyright (C) 2018, STMicroelectronics, all right reserved.
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_time       time 
#define mbedtls_time_t     time_t
#define mbedtls_fprintf    fprintf
#define mbedtls_printf     printf
#endif

#if !defined(MBEDTLS_BIGNUM_C) || !defined(MBEDTLS_ENTROPY_C) ||  \
    !defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_CLI_C) || \
    !defined(MBEDTLS_NET_C) || !defined(MBEDTLS_RSA_C) ||         \
    !defined(MBEDTLS_CERTS_C) || !defined(MBEDTLS_PEM_PARSE_C) || \
    !defined(MBEDTLS_CTR_DRBG_C) || !defined(MBEDTLS_X509_CRT_PARSE_C)
int main( void )
{
  mbedtls_printf("MBEDTLS_BIGNUM_C and/or MBEDTLS_ENTROPY_C and/or "
                 "MBEDTLS_SSL_TLS_C and/or MBEDTLS_SSL_CLI_C and/or "
                 "MBEDTLS_NET_C and/or MBEDTLS_RSA_C and/or "
                 "MBEDTLS_CTR_DRBG_C and/or MBEDTLS_X509_CRT_PARSE_C "
                 "not defined.\n");
  return( 0 );
}
#else

#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#include "mbedtls/memory_buffer_alloc.h"

#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <string.h>

#define SERVER_NAME "66.187.5.32"
#define SERVER_PORT "443"
#define GET_REQUEST "GET / HTTP/1.0\r\n\r\n"

static mbedtls_net_context server_fd;
static uint32_t flags;

uint8_t ssl_buf[1500];

static const  char *pers = "ssl_client";

uint8_t vrfy_buf[512];

int ret;

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;
mbedtls_x509_crt cacert;

/* use static allocation to keep the heap size as low as possible */
#ifdef MBEDTLS_MEMORY_BUFFER_ALLOC_C
uint8_t memory_buf[MAX_MEM_SIZE];
#endif

/* From https://letsencrypt.org/certificates/ */
/* Let��s Encrypt R3 (RSA 2048, O = Let's Encrypt, CN = R3) */
const char r3_ssl[] =
"-----BEGIN CERTIFICATE-----\r\n"                                   \
"MIIFFjCCAv6gAwIBAgIRAJErCErPDBinU/bWLiWnX1owDQYJKoZIhvcNAQELBQAw\r\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\r\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjAwOTA0MDAwMDAw\r\n" \
"WhcNMjUwOTE1MTYwMDAwWjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\r\n" \
"RW5jcnlwdDELMAkGA1UEAxMCUjMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\r\n" \
"AoIBAQC7AhUozPaglNMPEuyNVZLD+ILxmaZ6QoinXSaqtSu5xUyxr45r+XXIo9cP\r\n" \
"R5QUVTVXjJ6oojkZ9YI8QqlObvU7wy7bjcCwXPNZOOftz2nwWgsbvsCUJCWH+jdx\r\n" \
"sxPnHKzhm+/b5DtFUkWWqcFTzjTIUu61ru2P3mBw4qVUq7ZtDpelQDRrK9O8Zutm\r\n" \
"NHz6a4uPVymZ+DAXXbpyb/uBxa3Shlg9F8fnCbvxK/eG3MHacV3URuPMrSXBiLxg\r\n" \
"Z3Vms/EY96Jc5lP/Ooi2R6X/ExjqmAl3P51T+c8B5fWmcBcUr2Ok/5mzk53cU6cG\r\n" \
"/kiFHaFpriV1uxPMUgP17VGhi9sVAgMBAAGjggEIMIIBBDAOBgNVHQ8BAf8EBAMC\r\n" \
"AYYwHQYDVR0lBBYwFAYIKwYBBQUHAwIGCCsGAQUFBwMBMBIGA1UdEwEB/wQIMAYB\r\n" \
"Af8CAQAwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYfr52LFMLGMB8GA1UdIwQYMBaA\r\n" \
"FHm0WeZ7tuXkAXOACIjIGlj26ZtuMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcw\r\n" \
"AoYWaHR0cDovL3gxLmkubGVuY3Iub3JnLzAnBgNVHR8EIDAeMBygGqAYhhZodHRw\r\n" \
"Oi8veDEuYy5sZW5jci5vcmcvMCIGA1UdIAQbMBkwCAYGZ4EMAQIBMA0GCysGAQQB\r\n" \
"gt8TAQEBMA0GCSqGSIb3DQEBCwUAA4ICAQCFyk5HPqP3hUSFvNVneLKYY611TR6W\r\n" \
"PTNlclQtgaDqw+34IL9fzLdwALduO/ZelN7kIJ+m74uyA+eitRY8kc607TkC53wl\r\n" \
"ikfmZW4/RvTZ8M6UK+5UzhK8jCdLuMGYL6KvzXGRSgi3yLgjewQtCPkIVz6D2QQz\r\n" \
"CkcheAmCJ8MqyJu5zlzyZMjAvnnAT45tRAxekrsu94sQ4egdRCnbWSDtY7kh+BIm\r\n" \
"lJNXoB1lBMEKIq4QDUOXoRgffuDghje1WrG9ML+Hbisq/yFOGwXD9RiX8F6sw6W4\r\n" \
"avAuvDszue5L3sz85K+EC4Y/wFVDNvZo4TYXao6Z0f+lQKc0t8DQYzk1OXVu8rp2\r\n" \
"yJMC6alLbBfODALZvYH7n7do1AZls4I9d1P4jnkDrQoxB3UqQ9hVl3LEKQ73xF1O\r\n" \
"yK5GhDDX8oVfGKF5u+decIsH4YaTw7mP3GFxJSqv3+0lUFJoi5Lc5da149p90Ids\r\n" \
"hCExroL1+7mryIkXPeFM5TgO9r0rvZaBFOvV2z0gp35Z0+L4WPlbuEjN/lxPFin+\r\n" \
"HlUjr8gRsI3qfJOQFy/9rKIJR0Y/8Omwt/8oTWgy1mdeHmmjk7j1nYsvC9JSQ6Zv\r\n" \
"MldlTTKB3zhThV1+XWYp6rjd5JW1zbVWEkLNxE7GJThEUG3szgBVGP7pSWTUTsqX\r\n" \
"nLRbwHOoq7hHwg==\r\n" \
"-----END CERTIFICATE-----\r\n";


void SSL_Client(void *argument)
{
  int len;

  /*
   * 0. Initialize the RNG and the session data
   */
#ifdef MBEDTLS_MEMORY_BUFFER_ALLOC_C
  mbedtls_memory_buffer_alloc_init(memory_buf, sizeof(memory_buf));
#endif

  mbedtls_net_init(NULL);
  mbedtls_ssl_init(&ssl);
  mbedtls_ssl_config_init(&conf);
  mbedtls_x509_crt_init(&cacert);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  mbedtls_printf( "\n  . Seeding the random number generator..." );
 
  mbedtls_entropy_init( &entropy );
  len = strlen((char *)pers);
  if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, len)) != 0)
  {
    mbedtls_printf( " failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret );
    goto exit;
  }

  mbedtls_printf( " ok\n" );

  /*
   * 1. Initialize certificates
   */
  mbedtls_printf( "  . Loading the CA root certificate ..." );

  ret = mbedtls_x509_crt_parse( &cacert, (const unsigned char *) r3_ssl, sizeof(r3_ssl) );
  
  if( ret < 0 )
  {
    mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n", -ret );
    goto exit;
  }

  mbedtls_printf( " ok (%d skipped)\n", ret );

  /*
   * 2. Start the connection
   */
  mbedtls_printf( "  . Connecting to tcp/%s/%s...", SERVER_NAME, SERVER_PORT );
  
  if((ret = mbedtls_net_connect(&server_fd, SERVER_NAME, SERVER_PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
  {
    mbedtls_printf( " failed\n  ! mbedtls_net_connect returned %d\n\n", ret );
    goto exit;
  }

  mbedtls_printf( " ok\n" );

  /*
   * 3. Setup stuff
   */
  mbedtls_printf( "  . Setting up the SSL/TLS structure..." );
  
  if((ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
  {
    mbedtls_printf( " failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret );
    goto exit;
  }

  mbedtls_printf( " ok\n" );

  /* OPTIONAL is not optimal for security,
   * but makes interop easier in this simplified application */
  mbedtls_ssl_conf_authmode( &conf, MBEDTLS_SSL_VERIFY_OPTIONAL );
  mbedtls_ssl_conf_ca_chain( &conf, &cacert, NULL );
  mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );

  if((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
  {
    mbedtls_printf( " failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret );
    goto exit;
  }

  if((ret = mbedtls_ssl_set_hostname( &ssl, "mt.taterli.cyou" )) != 0)
  {
    mbedtls_printf( " failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret );
    goto exit;
  }

  mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

  /*
   * 4. Handshake
   */
  mbedtls_printf( "  . Performing the SSL/TLS handshake..." );

  while((ret = mbedtls_ssl_handshake( &ssl )) != 0)
  {
    if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
    {
      mbedtls_printf( " failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", -ret );
      goto exit;
    }
  }

  mbedtls_printf( " ok\n" );

  /*
   * 5. Verify the server certificate
   */
  mbedtls_printf( "  . Verifying peer X.509 certificate..." );

  if(( flags = mbedtls_ssl_get_verify_result( &ssl )) != 0)
  {
  
    mbedtls_printf( " failed\n" );
    mbedtls_x509_crt_verify_info((char *)vrfy_buf, sizeof( vrfy_buf ), "  ! ", flags);

    mbedtls_printf( "%s\n", vrfy_buf );
  }
  else
  {
    mbedtls_printf( " ok\n" );
  }
  
  /*
   * 6. Write the GET request
   */
  
  mbedtls_printf( "  > Write to server:" );
  
  sprintf((char *) ssl_buf, GET_REQUEST);
  len = strlen((char *) ssl_buf);
  
  while((ret = mbedtls_ssl_write(&ssl, ssl_buf, len)) <= 0)
  {
    if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE )
    {
      mbedtls_printf( " failed\n  ! mbedtls_ssl_write returned %d\n\n", ret );
      goto exit;
    }
  }

  len = ret;
  mbedtls_printf(" %d bytes written\n\n%s", len, (char *) ssl_buf);

  /*
   * 7. Read the HTTP response
   */
   mbedtls_printf("  < Read from server:");

  do
  {
    len = sizeof( ssl_buf ) - 1;
    memset( ssl_buf, 0, sizeof( ssl_buf ) );
    ret = mbedtls_ssl_read( &ssl, ssl_buf, len );

    if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
    {
      continue;
    }
    
    if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
    {
      break;
    }

    if(ret < 0)
    {
      mbedtls_printf( "failed\n  ! mbedtls_ssl_read returned %d\n\n", ret );
      break;
    }

    if(ret == 0)
    {
      mbedtls_printf( "\n\nEOF\n\n" );
    }

    len = ret;
    mbedtls_printf( " %d bytes read\n\n%s", len, (char *) ssl_buf );
  }
  while(1);

  mbedtls_ssl_close_notify( &ssl );

exit:
  mbedtls_net_free( &server_fd );

  mbedtls_x509_crt_free( &cacert );
  mbedtls_ssl_free( &ssl );
  mbedtls_ssl_config_free( &conf );
  mbedtls_ctr_drbg_free( &ctr_drbg );
  mbedtls_entropy_free( &entropy );
  
  if ((ret < 0) && (ret != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY))
  {
    vTaskDelay(5);
  }
  else
  {
    vTaskDelay(1);
  }
}

#endif /* MBEDTLS_BIGNUM_C && MBEDTLS_ENTROPY_C && MBEDTLS_SSL_TLS_C &&
          MBEDTLS_SSL_CLI_C && MBEDTLS_NET_C && MBEDTLS_RSA_C &&
          MBEDTLS_CERTS_C && MBEDTLS_PEM_PARSE_C && MBEDTLS_CTR_DRBG_C &&
          MBEDTLS_X509_CRT_PARSE_C */
