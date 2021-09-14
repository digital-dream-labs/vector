# Open Source information at Anki

-  [Open Source Software](https://ankiinc.atlassian.net/wiki/spaces/ET/pages/380436502/Open+Source+Software)
-  [Open Source License Go/Caution/Stop List (Distributed Software)](https://ankiinc.atlassian.net/wiki/spaces/ET/pages/380305449/Open+Source+License+Go+Caution+Stop+List+Distributed+Software)

## Using cmake to add licensing information to targets

- A target that comprises of only code written by Anki

  `anki_build_target_license(<target name> "ANKI")`

- An Open Source executable or library written by an outside source

  `anki_build_target_license(<target name> "BSD-2,${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")`

- An executable or library using a mix of Anki code and code from an external source

  `anki_build_target_license(<target name> "ANKI" "BSD-2,${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")`

  Although ideally each target would have it's own licensing information and code written by Anki is not mixed with code from external sources.

## Error messages

### `"WARNING: licensing information missing or not approved for TARGET target"`

  Licensing information is missing for this target, add with `anki_build_target_license(TARGET "<license>,<path to license>" "<license>,<path to license>")` or there is licensing information but the licenses are not approved.

### `"WARNING: missing path to license file for LICENSE on TARGET target"`

  Licensing information takes the form `<name of licence>,<path/to/license/file>`, in this instance only the name of the license has been supplied. Add the path to the license file, e.g.
  `anki_build_target_license(TARGET "<license>,<path to license>")`

### `"TARGET is using a relative path, FILE, for it's license"`

As a post-process all licenses are copied into ${CMAKE_BINARY_DIR} and this process needs an absolute path for the license files.

Instead of
  `anki_build_target_license(TARGET "FOO,license.txt")`
use
  `anki_build_target_license(TARGET "FOO,${CMAKE_CURRENT_SOURCE_DIR}/path/to/license.txt")`

### `"ERROR: unrecognised license LICENSE for TARGET target"`

The license name LICENSE is not on the list of licenses below.

### `"CAUTION: license LICENSE for TARGET needs approval"`

The license used is on the [caution list](https://ankiinc.atlassian.net/wiki/spaces/ET/pages/380436502/Open+Source+Software) and needs approval.

### `"STOP: license LICENSE for TARGET needs approval"`

A more severe warning than the previous message, the license used is on the [stop list](https://ankiinc.atlassian.net/wiki/spaces/ET/pages/380436502/Open+Source+Software) and needs approval.

### `"<library> is not a recognised system lib and does not have cmake configuration including licensing information"`

If this a new Vicos SDK library add it to the list of exceptions at https://github.com/anki/victor/blob/470d07552c8238c3fdc0301f86167daa409ff717/cmake/license.cmake#L324

# Licenses

## sources

https://ankiinc.atlassian.net/wiki/spaces/ET/pages/380305449/Open+Source+License+Go+Caution+Stop+List+Distributed+Software
source: git@github.com:/OpenSourceOrg/licenses.git f7ff223f9694ca0d5114fc82e43c74b5c5087891

Licenses may be:

- Go:         You do not need Legal approval to use code licensed under these licenses for all use cases.
- Caution:    You must obtain Legal approval to distribute code licensed under these licenses.
              You do not need Legal approval to use code licensed under these licenses internally.
- Stop:       You must obtain Legal approval to use any code licensed under these licenses.
- Reviewing:  It's use is under review
- Commercial: Under special conditions
- Ignore:     Licenses that do not require attribution (for example: MS dot net license)

Note: license identifier should be plain names as some characters have special meaning in either the local file system or when viewed a web URLs, e.g. avoid `/` and `+`

|Identifier    |Use       |Description                                                 | URL                                                                     |
|:------------:|:--------:| ---------------------------------------------------------- | ----------------------------------------------------------------------- |
|AAL           |          |Attribution Assurance License                               |https://opensource.org/licenses/AAL                                      |
|AFL-3.0       |          |Academic Free License, Version 3.0                          |https://opensource.org/licenses/AFL-3.0                                  |
|AGPL-3.0      |          |GNU AFFERO GENERAL PUBLIC LICENSE, Version 3 (AGPL-3.0)     |https://opensource.org/licenses/AGPL-3.0                                 |
|Apache-1.0    |Caution   |                                                            |                                                                         |
|Apache-1.1    |Go        |Apache Software License, Version 1.1                        |https://opensource.org/licenses/Apache-1.1                               |
|Apache-2.0    |Go        |Apache License, Version 2.0                                 |https://en.wikipedia.org/wiki/Apache_License                             |
|APL-1.0       |          |Adaptive Public License, Version 1.0                        |https://opensource.org/licenses/APL-1.0                                  |
|APSL-2.0      |          |Apple Public Source License, Version 2.0                    |https://opensource.org/licenses/APSL-2.0                                 |
|Artistic-1.0  |          |Artistic License, Version 1.0                               |https://opensource.org/licenses/Artistic-1.0                             |
|Artistic-2.0  |Go        |Artistic License, Version 2.0                               |https://opensource.org/licenses/Artistic-2.0                             |
|BSD-2         |Go        |BSD 2-Clause License                                        |https://en.wikipedia.org/wiki/BSD_licenses#2-clause                      |
|BSD-3         |Go        |BSD 3-Clause License                                        |https://en.wikipedia.org/wiki/BSD_licenses#3-clause                      |
|BSD-4         |Go        |                                                            |                                                                         |
|BSL-1.0       |Go        |Boost Software License 1.0 (BSL-1.0)                        |https://opensource.org/licenses/BSL-1.0                                  |
|CATOSL-1.1    |          |Computer Associates Trusted Open Source License, Version 1.1|https://opensource.org/licenses/CATOSL-1.1                               |
|CC0           |Go        |                                                            |https://creativecommons.org/share-your-work/public-domain/cc0/           |
|CCBY-SA       |Stop      |                                                            |                                                                         |
|CCBY          |Go        |                                                            |                                                                         |
|CDDL-1.0      |Caution   |Common Development and Distribution License, Version 1.0    |https://en.wikipedia.org/wiki/Common_Development_and_Distribution_License|
|CECILL-2.1    |          |Cea Cnrs Inria Logiciel Libre License, Version 2.1          |https://opensource.org/licenses/CECILL-2.1                               |
|CNRI-Python   |          |CNRI portion of the multi-part Python License               |https://opensource.org/licenses/CNRI-Python                              |
|Commercial    |Go        |                                                            |                                                                         |
|CPAL-1.0      |          |Common Public Attribution License Version 1.0 (CPAL-1.0)    |https://opensource.org/licenses/CPAL-1.0                                 |
|CPL-1.0       |Caution   |Common Public License, Version 1.0                          |https://opensource.org/licenses/CPL-1.0                                  |
|CUA-OPL-1.0   |          |CUA Office Public License                                   |https://opensource.org/licenses/CUA-OPL-1.0                              |
|curl          |          |The curl license (MIT/X Derivate)                           |https://curl.haxx.se/docs/copyright.html                                 |
|CVW           |          |The MITRE Collaborative Virtual Workspace License           |https://opensource.org/licenses/CVW                                      |
|ECL-1.0       |          |Educational Community License, Version 1.0                  |https://opensource.org/licenses/ECL-1.0                                  |
|ECL-2.0       |          |Educational Community License, Version 2.0                  |https://opensource.org/licenses/ECL-2.0                                  |
|EFL-1.0       |          |The Eiffel Forum License, Version 1                         |https://opensource.org/licenses/EFL-1.0                                  |
|EFL-2.0       |          |Eiffel Forum License, Version 2                             |https://opensource.org/licenses/EFL-2.0                                  |
|Entessa       |          |Entessa Public License                                      |https://opensource.org/licenses/Entessa                                  |
|EPL-1.0       |Caution   |Eclipse Public License, Version 1.0                         |https://en.wikipedia.org/wiki/Eclipse_Public_License                     |
|EUDatagrid    |          |EU DataGrid Software License                                |https://opensource.org/licenses/EUDatagrid                               |
|EUPL-1.1      |          |European Union Public License, Version 1.1                  |https://opensource.org/licenses/EUPL-1.1                                 |
|Fair          |          |Fair License (Fair)                                         |https://opensource.org/licenses/Fair                                     |
|FFTPACK5      |Go        |BSD-style                                                   |https://www2.cisl.ucar.edu/resources/legacy/fft5/license                 |
|Frameworx-1.0 |          |Frameworx License, Version 1.0                              |https://opensource.org/licenses/Frameworx-1.0                            |
|GFDL-1.2      |          |                                                            |                                                                         |
|GFDL-1.3      |          |                                                            |                                                                         |
|GPL-1.0       |          |                                                            |                                                                         |
|GPL-2.0       |Caution   |GNU General Public License, Version 2.0                     |https://en.wikipedia.org/wiki/GNU_General_Public_License                 |
|GPL-3.0       |Stop      |GNU General Public License, Version 3.0                     |https://en.wikipedia.org/wiki/GNU_General_Public_License                 |
|HPND          |          |Historical Permission Notice and Disclaimer                 |https://opensource.org/licenses/HPND                                     |
|Intel         |          |The Intel Open Source License                               |https://opensource.org/licenses/Intel                                    |
|IPA           |          |IPA Font License                                            |https://opensource.org/licenses/IPA                                      |
|IPL-1.0       |Caution   |IBM Public License, Version 1.0                             |https://opensource.org/licenses/IPL-1.0                                  |
|ISC:wraptext  |          |                                                            |                                                                         |
|ISC           |GO        |ISC License (ISC)                                           |https://en.wikipedia.org/wiki/ISC_license                                |
|jabberpl      |          |Jabber Open Source License                                  |https://opensource.org/licenses/jabberpl                                 |
|libpng        |Go        |                                                            |https://en.wikipedia.org/wiki/Libpng_License                             |
|LGPL-2.0      |          |                                                            |                                                                         |
|LGPL-2.1      |Caution   |GNU Lesser General Public License, Version 2.1              |https://en.wikipedia.org/wiki/GNU_Lesser_General_Public_License          |
|LGPL-3.0      |Stop      |GNU Lesser General Public License, Version 3.0              |https://en.wikipedia.org/wiki/GNU_Lesser_General_Public_License          |
|LiLiQ-P-1.1   |          |Licence Libre du Québec – Permissive, Version 1.1           |https://opensource.org/licenses/LiLiQ-P-1.1                              |
|LiLiQ-R+      |          |Licence Libre du Québec – Réciprocité forte, Version 1.1    |https://opensource.org/licenses/LiLiQ-Rplus-1.1                          |
|LiLiQ-R-1.1   |          |Licence Libre du Québec – Réciprocité, Version 1.1          |https://opensource.org/licenses/LiLiQ-R-1.1                              |
|LPL-1.02      |          |Lucent Public License, Version 1.02                         |https://opensource.org/licenses/LPL-1.02                                 |
|LPL-1.0       |          |Lucent Public License, Plan 9, Version 1.0                  |https://opensource.org/licenses/LPL-1.0                                  |
|LPPL-1.3c     |          |LaTeX Project Public License, Version 1.3c                  |https://opensource.org/licenses/LPPL-1.3c                                |
|MirOS         |          |The MirOS Licence (MirOS)                                   |https://opensource.org/licenses/MirOS                                    |
|MIT           |Go        |MIT/Expat License                                           |https://en.wikipedia.org/wiki/MIT_License                                |
|Motosoto      |          |Motosoto Open Source License, Version 0.9.1                 |https://opensource.org/licenses/Motosoto                                 |
|MPL-1.0       |          |Mozilla Public License, Version 1.0                         |https://opensource.org/licenses/MPL-1.0                                  |
|MPL-1.1       |Caution   |Mozilla Public License, Version 1.1                         |https://opensource.org/licenses/MPL-1.1                                  |
|MPL-2.0       |Caution   |Mozilla Public License, Version 2.0                         |https://en.wikipedia.org/wiki/MPL_License                                |
|MS-PL         |Go        |Microsoft Public License (MS-PL)                            |https://opensource.org/licenses/MS-PL                                    |
|MS-RL         |          |Microsoft Reciprocal License (MS-RL)                        |https://opensource.org/licenses/MS-RL                                    |
|MS License    |Ignore    |MICROSOFT .NET LIBRARY SOFTWARE LICENSE TERMS               |https://www.microsoft.com/net/dotnet_library_license.htm                 |
|Multics       |          |Multics License                                             |https://opensource.org/licenses/Multics                                  |
|NASA-1.3      |          |NASA Open Source Agreement, Version 1.3                     |https://opensource.org/licenses/NASA-1.3                                 |
|Naumen        |          |NAUMEN Public License                                       |https://opensource.org/licenses/Naumen                                   |
|NCSA          |          |The University of Illinois/NCSA Open Source License         |https://opensource.org/licenses/NCSA                                     |
|NGPL          |          |The Nethack General Public License                          |https://opensource.org/licenses/NGPL                                     |
|Nokia         |          |Nokia Open Source License, Version 1.0a                     |https://opensource.org/licenses/Nokia                                    |
|NPOSL-3.0     |          |The Non-Profit Open Software License, Version 3.0           |https://opensource.org/licenses/NPOSL-3.0                                |
|NTP           |          |NTP License (NTP)                                           |https://opensource.org/licenses/NTP                                      |
|OCLC-2.0      |          |The OCLC Research Public License, Version 2.0               |https://opensource.org/licenses/OCLC-2.0                                 |
|OFL-1.1       |          |SIL Open Font License, Version 1.1                          |https://opensource.org/licenses/OFL-1.1                                  |
|OGTSL         |          |The Open Group Test Suite License (OGTSL)                   |https://opensource.org/licenses/OGTSL                                    |
|OpenSSL-SSLeay|          |Dual: OpenSSL (Apache-style) and SSLeay (BSD-style)         |https://www.openssl.org/source/license.txt                               |
|OPL-2.1       |          |OSET Foundation Public License                              |https://www.osetfoundation.org/public-license                            |
|OSL-1.0       |          |Open Software License, Version 1.0                          |https://opensource.org/licenses/OSL-1.0                                  |
|OSL-2.1       |          |Open Software License, Version 2.1                          |https://opensource.org/licenses/OSL-2.1                                  |
|OSL-3.0       |          |Open Software License, Version 3.0                          |https://opensource.org/licenses/OSL-3.0                                  |
|PHP-3.0       |Go        |The PHP License, Version 3.0                                |https://opensource.org/licenses/PHP-3.0                                  |
|PostgreSQL    |          |The PostgreSQL Licence                                      |https://opensource.org/licenses/PostgreSQL                               |
|Public Domain |Go        |                                                            |                                                                         |
|Python-2.0    |Go        |Python License, Version 2.0                                 |https://opensource.org/licenses/Python-2.0                               |
|QPL-1.0       |          |The Q Public License Version (QPL-1.0)                      |https://opensource.org/licenses/QPL-1.0                                  |
|RPL-1.1       |          |Reciprocal Public License, Version 1.1                      |https://opensource.org/licenses/RPL-1.1                                  |
|RPL-1.5       |          |Reciprocal Public License, Version 1.5                      |https://opensource.org/licenses/RPL-1.5                                  |
|RPSL-1.0      |          |RealNetworks Public Source License, Version 1.0             |https://opensource.org/licenses/RPSL-1.0                                 |
|RSA           |Go        |RSA Data Security, Inc.                                     |                                                                         |
|RSCPL         |          |The Ricoh Source Code Public License                        |https://opensource.org/licenses/RSCPL                                    |
|SAAS          |Stop      |                                                            |                                                                         |
|Simple-2.0    |          |Simple Public License (SimPL-2.0)                           |https://opensource.org/licenses/Simple-2.0                               |
|SISSL         |          |Sun Industry Standards Source License                       |https://opensource.org/licenses/SISSL                                    |
|Sleepycat     |Stop      |The Sleepycat License                                       |https://opensource.org/licenses/Sleepycat                                |
|SPL-1.0       |          |Sun Public License, Version 1.0                             |https://opensource.org/licenses/SPL-1.0                                  |
|Unlicense     |Go        |                                                            |                                                                         |
|UPL           |          |The Universal Permissive License (UPL), Version 1.          |https://opensource.org/licenses/UPL                                      |
|VSL-1.0       |          |The Vovida Software License, Version 1.0                    |https://opensource.org/licenses/VSL-1.0                                  |
|W3C           |          |The W3C Software Notice and License                         |https://opensource.org/licenses/W3C                                      |
|Watcom-1.0    |          |The Sybase Open Source Licence                              |https://opensource.org/licenses/Watcom-1.0                               |
|WTFPL         |Go        |                                                            |                                                                         |
|WXwindows     |          |The wxWindows Library Licence                               |https://opensource.org/licenses/WXwindows                                |
|Xiph.org      |Go        |Xiph.org Foundation                                         |                                                                         |
|Xnet          |          |The X.Net, Inc. License                                     |https://opensource.org/licenses/Xnet                                     |
|Zlib          |Go        |The zlib/libpng License (Zlib)                              |https://opensource.org/licenses/Zlib                                     |
|ZPL-2.0       |          |The Zope Public License, Version 2.0                        |https://opensource.org/licenses/ZPL-2.0                                  |


## Target overrides

**DO NOT ADD TO THIS LIST UNLESS INSTRUCTED BY A DIRECTOR.**

[//]: # (overrides_table)

|Target        |License   |Notes                                                            |
|:------------:|:--------:| --------------------------------------------------------------- |
|mpg123        |LGPL-2.1  | Dynamic linkage only! Source must be included in engine tarball |
