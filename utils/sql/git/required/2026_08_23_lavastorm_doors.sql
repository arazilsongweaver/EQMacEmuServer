  UPDATE `doors`
  SET
      `doorid` = 2,
      `zone` = 'lavastorm',
      `name` = 'SOLPLUG',
      `pos_y` = 1354.9222412109375,
      `pos_x` = 331.14019775390625,
      `pos_z` = 122.18850708007812,
      `heading` = 0,
      `opentype` = 57,
      `lockpick` = 0,
      `keyitem` = 0,
      `altkeyitem` = 0,
      `nokeyring` = 1,
      `triggerdoor` = 0,
      `triggertype` = 0,
      `doorisopen` = 0,
      `door_param` = 88,
      `dest_zone` = 'soltemple',
      `dest_x` = 56,
      `dest_y` = 250,
      `dest_z` = 3,
      `dest_heading` = 999,
      `invert_state` = 0,
      `incline` = 0,
      `size` = 100,
      `client_version_mask` = 4294967295,
      `islift` = 0,
      `close_time` = 5,
      `can_open` = 1,
      `min_expansion` = -1,
      `max_expansion` = -1,
      `content_flags` = NULL,
      `content_flags_disabled` = NULL
  WHERE `id` = 35877;

  UPDATE `doors`
  SET
      `doorid` = 3,
      `zone` = 'lavastorm',
      `name` = 'SOLPLUG',
      `pos_y` = 1359.675048828125,
      `pos_x` = 331.1307067871094,
      `pos_z` = 145.18951416015625,
      `heading` = 0,
      `opentype` = 57,
      `lockpick` = 0,
      `keyitem` = 0,
      `altkeyitem` = 0,
      `nokeyring` = 0,
      `triggerdoor` = 0,
      `triggertype` = 0,
      `doorisopen` = 0,
      `door_param` = 77,
      `dest_zone` = 'soltemple',
      `dest_x` = 56,
      `dest_y` = 250,
      `dest_z` = 3,
      `dest_heading` = 999,
      `invert_state` = 0,
      `incline` = 0,
      `size` = 100,
      `client_version_mask` = 4294967295,
      `islift` = 0,
      `close_time` = 5,
      `can_open` = 1,
      `min_expansion` = -1,
      `max_expansion` = -1,
      `content_flags` = NULL,
      `content_flags_disabled` = NULL
  WHERE `id` = 35884;

  UPDATE `doors`
  SET
      `doorid` = 4,
      `zone` = 'lavastorm',
      `name` = 'SOLPLUG',
      `pos_y` = 908.040771484375,
      `pos_x` = 484.2760925292969,
      `pos_z` = 15.438499450683594,
      `heading` = 249.88003540039062,
      `opentype` = 57,
      `lockpick` = 0,
      `keyitem` = 0,
      `altkeyitem` = 0,
      `nokeyring` = 1,
      `triggerdoor` = 0,
      `triggertype` = 0,
      `doorisopen` = 0,
      `door_param` = 2,
      `dest_zone` = 'lavastorm',
      `dest_x` = 483,
      `dest_y` = 905,
      `dest_z` = 57,
      `dest_heading` = 999,
      `invert_state` = 0,
      `incline` = 0,
      `size` = 100,
      `client_version_mask` = 4294967295,
      `islift` = 0,
      `close_time` = 5,
      `can_open` = 1,
      `min_expansion` = -1,
      `max_expansion` = -1,
      `content_flags` = NULL,
      `content_flags_disabled` = NULL
  WHERE `id` = 35879;

  INSERT INTO `zone_points`
  (
      `zone`,
      `number`,
      `y`,
      `x`,
      `z`,
      `heading`,
      `target_y`,
      `target_x`,
      `target_z`,
      `target_heading`,
      `target_zone_id`,
      `client_version_mask`,
      `min_expansion`,
      `max_expansion`,
      `content_flags`,
      `content_flags_disabled`,
      `is_virtual`,
      `height`,
      `width`
  )
  VALUES
  (
      'lavastorm',
      2,
      0,
      0,
      0,
      0,
      905,
      483,
      57,
      999,
      27,
      4294967295,
      -1,
      -1,
      NULL,
      NULL,
      0,
      0,
      0
  );
  
  -- the plugs and zone points to soltemple both depend on the content flag.
  -- when they aren't present, the baked in DRNTP_ZONE that's normally not
  -- reachable behind the SOLPLUG becomes accessible and the client takes 
  -- the player to the safe point after the server declines the zone ID 0 request.

  UPDATE `doors`
  SET
      `content_flags` = 'OldPlane_Hate_Sky',
      `content_flags_disabled` = NULL
  WHERE `zone` = 'lavastorm'
    AND `doorid` IN (2, 3);

  UPDATE `zone_points`
  SET
      `content_flags` = 'OldPlane_Hate_Sky',
      `content_flags_disabled` = NULL
  WHERE `zone` = 'lavastorm'
    AND `number` IN (77, 88);
