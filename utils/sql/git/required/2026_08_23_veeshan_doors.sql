  UPDATE doors
  SET door_param = CASE
      WHEN doorid = 194 AND dest_zone = 'airplane'      THEN 1
      WHEN doorid = 125 AND dest_zone = 'freporte'      THEN 2
      WHEN doorid = 129 AND dest_zone = 'skyfire'       THEN 3
      WHEN doorid = 86  AND dest_zone = 'swampofnohope' THEN 4
      ELSE door_param
  END
  WHERE zone = 'veeshan'
    AND opentype = 57
    AND NAME = 'VETELE101'
    AND (
         (doorid = 194 AND dest_zone = 'airplane')
      OR (doorid = 125 AND dest_zone = 'freporte')
      OR (doorid = 129 AND dest_zone = 'skyfire')
      OR (doorid = 86  AND dest_zone = 'swampofnohope')
    );

  UPDATE doors
  SET dest_heading = CASE doorid
      WHEN 125 THEN 256
      WHEN 129 THEN 462
      WHEN 86  THEN 462
  END
  WHERE zone = 'veeshan'
    AND opentype = 57
    AND NAME = 'VETELE101'
    AND (
         (doorid = 125 AND dest_zone = 'freporte')
      OR (doorid = 129 AND dest_zone = 'skyfire')
      OR (doorid = 86  AND dest_zone = 'swampofnohope')
    );
