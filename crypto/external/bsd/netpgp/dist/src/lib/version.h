/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef VERSION_H_
#define VERSION_H_	1

#ifndef NETPGP_AUTOCONF_VERSION
#define NETPGP_AUTOCONF_VERSION	PACKAGE_VERSION
#endif

#ifndef NETPGP_MAINTAINER
#define NETPGP_MAINTAINER	PACKAGE_BUGREPORT
#endif

/* development versions have .99 suffix */
#define NETPGP_BASE_VERSION	"0.99.1"

#define NETPGP_VERSION_CAT(a, b)	"NetPGP portable " a "/[" b "]"
#define NETPGP_VERSION_STRING \
	NETPGP_VERSION_CAT(NETPGP_BASE_VERSION, NETPGP_AUTOCONF_VERSION)

#endif /* !VERSION_H_ */
