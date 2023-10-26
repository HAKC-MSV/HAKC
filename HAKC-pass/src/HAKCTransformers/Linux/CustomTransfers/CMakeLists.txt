target_sources(HAKC-Compartmentalizer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_sk_buff.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_file.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_socket.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_fuse_mount.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_scsi_cmnd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_usb_device.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_usb_interface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_us_data.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/CustomTransfer_scsi_device.cpp
        )
