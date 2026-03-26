// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2019 Red Hat, Inc.
 */

#include "fix_coverity.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

#include "efiboot.h"

int HIDDEN
parse_acpi_hid_uid(struct device *dev, const char *fmt, ...)
{
	int rc;
	ssize_t fbufsiz;
	char *path = NULL;
	va_list ap;
	char *fbuf = NULL;
	const char *gaze = NULL;
	uint16_t tmp16;
	uint32_t acpi_hid = 0;
	uint64_t acpi_uid_int = 0;

	debug("entry");

	va_start(ap, fmt);
	rc = vasprintfa(&path, fmt, ap);
	va_end(ap);
	debug("path:%s rc:%d", path, rc);
	if (rc < 0 || path == NULL)
		return -1;

	fbufsiz = read_sysfs_file(&fbuf, "%s/firmware_node/path", path);
	if (fbufsiz > 0 && fbuf) {
		if (fbufsiz > 2) {   // 2 == "\n\0"
			fbuf[fbufsiz-2] = 0;	// Zap trailing newline
			dev->acpi_root.acpi_cid_str = strdup(fbuf);
			debug("Setting ACPI root path to '%s'", fbuf);
		}
	}

	fbufsiz = read_sysfs_file(&fbuf, "%s/firmware_node/hid", path);
	if (fbufsiz < 0 || fbuf == NULL) {
		efi_error("could not read %s/firmware_node/hid", path);
		return -1;
	}

	if (fbufsiz < 6) {	// 6 = "0123\n\0"
hid_err:
		efi_error("could not parse %s/firmware_node/hid", path);
		return -1;
	}
	fbuf[fbufsiz-2] = 0;		// Zap trailing newline
	gaze = fbuf + fbufsiz - 6;	// Focus on the 4 hex digits at the end

	rc = sscanf(gaze, "%04hx", &tmp16);
	debug("rc:%d hid:0x%04x (from '%s')\n", rc, tmp16, fbuf);
	if (rc != 1)
		goto hid_err;

	if (strncmp(fbuf, "PNP", 3) == 0) {
		acpi_hid = EFIDP_EFI_PNP_ID(tmp16);
	} else if (strncmp(fbuf, "ACPI", 4) == 0) {
		acpi_hid = EFIDP_EFI_ACPI_ID(tmp16);
	} else {
		goto hid_err;
	}

	/*
	 * Apparently basically nothing can look up a PcieRoot() node,
	 * because they just check _CID.  So since _CID for the root pretty
	 * much always has to be PNP0A03 anyway, just use that no matter
	 * what.
	 */
	if (acpi_hid == EFIDP_ACPI_PCIE_ROOT_HID)
		acpi_hid = EFIDP_ACPI_PCI_ROOT_HID;
	dev->acpi_root.acpi_hid = acpi_hid;
	debug("acpi root HID:0x%08x", acpi_hid);

	errno = 0;
	fbuf = NULL;
	fbufsiz = read_sysfs_file(&fbuf, "%s/firmware_node/uid", path);
	if ((fbufsiz < 0 && errno != ENOENT) || (fbufsiz > 0 && fbuf == NULL)) {
		efi_error("could not read %s/firmware_node/uid", path);
		return -1;
	}
	if (fbufsiz > 0) {
		rc = sscanf((char *)fbuf, "%"PRIu64"\n", &acpi_uid_int);
		if (rc == 1) {
			dev->acpi_root.acpi_uid = acpi_uid_int;
		} else {
			/* kernel uses "%s\n" to print it, so there
			 * should always be some value and a newline... */
			if (fbufsiz > 2) {   // 2 == "\n\0"
				fbuf[fbufsiz-2] = '\0';
				dev->acpi_root.acpi_uid_str = strdup(fbuf);
			}
		}
	}
	debug("acpi root UID:0x%"PRIx64" uidstr:'%s'",
	      dev->acpi_root.acpi_uid, dev->acpi_root.acpi_uid_str);

	errno = 0;
	return 0;
}

// vim:fenc=utf-8:tw=75:noet
