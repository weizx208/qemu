system_ss.add(files(
  'block.c',
  'cdrom.c',
  'hd-geometry.c'
))

system_ss.add(when: 'CONFIG_PL35X', if_true: files('pl35x.c'))

system_ss.add(when: 'CONFIG_XLNX_VERSAL_PMC', if_true: files(
  'ufshc-if.c',
  'ufshc.c',
  'ufs-scsi-if.c',
  'ufs-scsi-core.c'
))

system_ss.add(when: 'CONFIG_M24CXX', if_true: files('m24cxx.c'))
system_ss.add(when: 'CONFIG_ARASAN_NFC', if_true: files('arasan_nfc.c'))
system_ss.add(when: 'CONFIG_FDC', if_true: files('fdc.c'))
system_ss.add(when: 'CONFIG_FDC_ISA', if_true: files('fdc-isa.c'))
system_ss.add(when: 'CONFIG_FDC_SYSBUS', if_true: files('fdc-sysbus.c'))
system_ss.add(when: 'CONFIG_PFLASH_CFI01', if_true: files('pflash_cfi01.c'))
system_ss.add(when: 'CONFIG_PFLASH_CFI02', if_true: files('pflash_cfi02.c'))
system_ss.add(when: 'CONFIG_SSI_M25P80', if_true: files('m25p80.c'))
system_ss.add(when: 'CONFIG_SSI_M25P80', if_true: files('m25p80_sfdp.c'))
system_ss.add(when: 'CONFIG_SWIM', if_true: files('swim.c'))
system_ss.add(when: 'CONFIG_XEN_BUS', if_true: files('xen-block.c'))

specific_ss.add(when: 'CONFIG_VIRTIO_BLK', if_true: files('virtio-blk.c'))
system_ss.add(when: 'CONFIG_VIRTIO_BLK', if_true: files('virtio-blk-common.c'))
specific_ss.add(when: 'CONFIG_VHOST_USER_BLK', if_true: files('vhost-user-blk.c'))
system_ss.add(when: 'CONFIG_VHOST_USER_BLK', if_true: files('virtio-blk-common.c'))

subdir('dataplane')
