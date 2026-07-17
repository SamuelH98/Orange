Orange has been installed.

Run the setup helper to configure GDM, dconf, AccountsService,
and the GDM runit service:

  # orange-setup

Then enable the required services:

  # ln -s /etc/sv/dbus /var/service/
  # ln -s /etc/sv/elogind /var/service/
  # ln -s /etc/sv/seatd /var/service/
  # ln -s /etc/sv/gdm /var/service/

Then reboot. Select "Orange" from the GDM session menu.
