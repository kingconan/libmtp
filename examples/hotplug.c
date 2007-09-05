/** 
 * \file hotplug.c
 * Example program to create hotplug scripts.
 *
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2006 Marcus Meissner <marcus@jet.franken.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "common.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void usage(void)
{
  fprintf(stderr, "usage: hotplug [-u -H -i -a\"ACTION\"]\n");
  fprintf(stderr, "       -u:  use udev syntax\n");
  fprintf(stderr, "       -U:  use newer udev syntax (SYSFS -> ATTR)\n");
  fprintf(stderr, "       -H:  use hal syntax\n");
  fprintf(stderr, "       -i:  use usb.ids simple list syntax\n");
  fprintf(stderr, "       -a\"ACTION\": perform udev action ACTION on attachment\n");
  exit(1);
}

enum style {
  style_usbmap,
  style_udev,
  style_udev_new,
  style_hal,
  style_usbids
};

int main (int argc, char **argv)
{
  LIBMTP_device_entry_t *entries;
  int numentries;
  int i;
  int ret;
  enum style style = style_usbmap;
  int opt;
  extern int optind;
  extern char *optarg;
  char *udev_action = NULL;
  char default_udev_action[] = "SYMLINK+=\"libmtp-%k\", MODE=\"666\"";
  uint16_t last_vendor = 0x0000U;

  while ( (opt = getopt(argc, argv, "uUiHa:")) != -1 ) {
    switch (opt) {
    case 'a':
      udev_action = strdup(optarg);
    case 'u':
      style = style_udev;
      break;
    case 'U':
      style = style_udev_new;
      break;
    case 'H':
      style = style_hal;
      break;
    case 'i':
      style = style_usbids;
      break;
    default:
      usage();
    }
  }

  LIBMTP_Init();
  ret = LIBMTP_Get_Supported_Devices_List(&entries, &numentries);
  if (ret == 0) {
    switch (style) {
    case style_udev:
    case style_udev_new:
      printf("# UDEV-style hotplug map for libmtp\n");
      printf("# Put this file in /etc/udev/rules.d\n\n");
      printf("ACTION!=\"add\", GOTO=\"libmtp_rules_end\"\n");
      if (style == style_udev_new) {
	printf("ATTR{dev}!=\"?*\", GOTO=\"libmtp_rules_end\"\n");
      }
      printf("SUBSYSTEM==\"usb\", GOTO=\"libmtp_rules\"\n"
	     "# The following line will be deprecated when older kernels are phased out.\n"
             "SUBSYSTEM==\"usb_device\", GOTO=\"libmtp_rules\"\n\n"
	     "GOTO=\"libmtp_rules_end\"\n\n"
	     "LABEL=\"libmtp_rules\"\n\n");
      break;
    case style_usbmap:
      printf("# This usermap will call the script \"libmtp.sh\" whenever a known MTP device is attached.\n\n");
      break;
    case style_hal:
      printf("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?> <!-- -*- SGML -*- -->\n");
      printf("<!-- This file was generated by %s - - fdi -->\n", argv[0]);
      printf("<deviceinfo version=\"0.2\">\n");
      printf("  <device>\n");
      printf("    <match key=\"info.bus\" string=\"usb\">\n");
      break;
    case style_usbids:
      printf("# usb.ids style device list from libmtp\n");
      printf("# Compare: http://www.linux-usb.org/usb.ids\n");
      break;
    }

    for (i = 0; i < numentries; i++) {
      LIBMTP_device_entry_t * entry = &entries[i];

      switch (style) {
      case style_udev: 
      case style_udev_new:
	{
          char *action;
          printf("# %s\n", entry->name);
          if (udev_action != NULL) {
            action = udev_action;
          } else {
            action = default_udev_action;
          }
	  if (style == style_udev) {
	    // Old style directly SYSFS named.
	    printf("SYSFS{idVendor}==\"%04x\", SYSFS{idProduct}==\"%04x\", %s\n", entry->vendor_id, entry->product_id, action);
	  } else {
	    // Newer style
	    printf("ATTR{idVendor}==\"%04x\", ATTR{idProduct}==\"%04x\", %s\n", entry->vendor_id, entry->product_id, action);
	  }
	  break;
        }
      case style_usbmap:
          printf("# %s\n", entry->name);
          printf("libmtp.sh    0x0003  0x%04x  0x%04x  0x0000  0x0000  0x00    0x00    0x00    0x00    0x00    0x00    0x00000000\n", entry->vendor_id, entry->product_id);
          break;
        case style_hal:
          printf("      <match key=\"usb.vendor_id\" int=\"0x%04x\">\n", entry->vendor_id);
          printf("        <match key=\"usb.product_id\" int=\"0x%04x\">\n", entry->product_id);
          /* FIXME: If hal >=0.5.10 can be depended upon, the matches below with contains_not can instead use addset */
          printf("          <match key=\"info.capabilities\" contains_not=\"portable_audio_player\">\n");
          printf("            <append key=\"info.capabilities\" type=\"strlist\">portable_audio_player</append>\n");
          printf("          </match>\n");
          printf("          <merge key=\"info.category\" type=\"string\">portable_audio_player</merge>\n");
          printf("          <merge key=\"portable_audio_player.access_method\" type=\"string\">user</merge>\n");
          printf("          <match key=\"portable_audio_player.access_method.protocols\" contains_not=\"mtp\">\n");
          printf("            <append key=\"portable_audio_player.access_method.protocols\" type=\"strlist\">mtp</append>\n");
          printf("          </match>\n");
          printf("          <append key=\"portable_audio_player.access_method.drivers\" type=\"strlist\">libmtp</append>\n");
          /* FIXME: needs true list of formats ... But all of them can do MP3 and WMA */
          printf("          <match key=\"portable_audio_player.output_formats\" contains_not=\"audio/mpeg\">\n");
          printf("            <append key=\"portable_audio_player.output_formats\" type=\"strlist\">audio/mpeg</append>\n");
          printf("          </match>\n");
          printf("          <match key=\"portable_audio_player.output_formats\" contains_not=\"audio/x-ms-wma\">\n");
          printf("            <append key=\"portable_audio_player.output_formats\" type=\"strlist\">audio/x-ms-wma</append>\n");
          printf("          </match>\n");
	  /* Special hack to support the OGG format - irivers, TrekStor and NormSoft (Palm) can always play these files! */
	  if (entry->vendor_id == 0x4102 || // iriver
	      entry->vendor_id == 0x066f || // TrekStor
	      entry->vendor_id == 0x1703) { // NormSoft, Inc.
	    printf("          <match key=\"portable_audio_player.output_formats\" contains_not=\"application/ogg\">\n");
	    printf("            <append key=\"portable_audio_player.output_formats\" type=\"strlist\">application/ogg</append>\n");
	    printf("          </match>\n");
	  }
          printf("          <merge key=\"portable_audio_player.libmtp.protocol\" type=\"string\">mtp</merge>\n");
          printf("          <merge key=\"portable_audio_player.libmtp.name\" type=\"string\">%s</merge>\n", entry->name);
          printf("        </match>\n");
          printf("      </match>\n");
        break;
        case style_usbids:
          if (last_vendor != entry->vendor_id) {
            printf("%04x\n", entry->vendor_id);
          }
          printf("\t%04x  %s\n", entry->product_id, entry->name);
        break;
      }
      last_vendor = entry->vendor_id;
    }
  } else {
    printf("Error.\n");
    exit(1);
  }

  switch (style) {
  case style_usbmap:
    break;
  case style_udev:
    printf("\nLABEL=\"libmtp_rules_end\"\n");
    break;
  case style_hal:
    printf("    </match>\n");
    printf("  </device>\n");
    printf("</deviceinfo>\n");
    break;
  case style_usbids:
    printf("\n");
  }

  exit (0);
}
