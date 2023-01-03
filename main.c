#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

#define GET 0
#define SET 1
#define LIST 2
#define BRIGHTNESS 0x10
#define CONTRAST 0x12
#define VOLUME 0x62
#define ALL_TARGETS 0x33

uint16_t m_action;
uint16_t m_feature;
uint16_t m_target = 1; // defaults to Display 1
uint16_t m_value;
int m_change = 0;

/******************************************************************************
function:	get_value
parameter: DDCA_Display_Handle dh, DDCA_Vcp_Feature_Code m_feature
******************************************************************************/
uint16_t get_value(DDCA_Display_Handle dh, DDCA_Vcp_Feature_Code m_feature)
{
   DDCA_Status ddcrc0;
   DDCA_Non_Table_Vcp_Value valrec;
   ddcrc0 = ddca_get_non_table_vcp_value(
       dh,
       m_feature,
       &valrec);
   if (ddcrc0 != 0)
   {
      fprintf(stderr, "ERROR: unable to get vcp value\n");
      return -1;
   }
   return valrec.sh << 8 | valrec.sl;
}

/******************************************************************************
function:	set_value
parameter: DDCA_Display_Handle dh, DDCA_Vcp_Feature_Code m_feature, uint16_t m_value
******************************************************************************/
DDCA_Status set_value(DDCA_Display_Handle dh, DDCA_Vcp_Feature_Code m_feature, uint16_t m_value)
{
   uint8_t hi_byte = m_value >> 8;
   uint8_t lo_byte = m_value & 0xff;
   DDCA_Status ddcrc = ddca_set_non_table_vcp_value(dh, m_feature, hi_byte, lo_byte);
   if (ddcrc == DDCRC_VERIFY)
   {
      fprintf(stderr, "ERROR: Value verification failed.\n");
   }
   else if (ddcrc != 0)
   {
      fprintf(stderr, "ERROR: unable to set vcp value\n");
   }
   return ddcrc;
}

/******************************************************************************
function:	shift_args
parameter: int *argc, char ***argv
******************************************************************************/
static char *shift_args(int *argc, char ***argv)
{
   assert(*argc > 0);
   char *result = **argv;
   *argc -= 1;
   *argv += 1;
   return result;
}

/******************************************************************************
function:	usage
parameter: const char *program
******************************************************************************/
void usage(const char *program)
{
   fprintf(stderr, "Usage: %s <ACTION> [TARGET] [FEATURE] [VALUE][+-]\n", program);
   fprintf(stderr, "\nACTION (Required):\n");
   fprintf(stderr, "    get:           get a value\n");
   fprintf(stderr, "    set:           set a value\n");
   fprintf(stderr, "    list:          list valid displays\n");
   fprintf(stderr, "    help:          display program usage\n");
   fprintf(stderr, "\nTARGET (Optional display number, defaults to first valid display):\n");
   fprintf(stderr, "    1 .. N         display number reported by `list` ACTION\n");
   fprintf(stderr, "    all            perform ACTION FEATURE for all valid displays\n");
   fprintf(stderr, "\nFEATURE (Required for `get` and `set` ACTION):\n");
   fprintf(stderr, "    brightness:    get/set brightness\n");
   fprintf(stderr, "    contrast:      get/set contrast\n");
   fprintf(stderr, "    volume:        get/set volume\n");
   fprintf(stderr, "\nVALUE (Required for `set` ACTION):\n");
   fprintf(stderr, "    0 .. 100[+-]   optional trailing `+` or `-` indicates\n");
   fprintf(stderr, "                   an incremental change");
   fprintf(stderr, "\nExamples:\n");
   fprintf(stderr, "    $ %s list\n", program);
   fprintf(stderr, "    $ %s get all brightness\n", program);
   fprintf(stderr, "    $ %s set 1 contrast 60\n", program);
   fprintf(stderr, "    $ %s set volume 10+\n", program);
}

/******************************************************************************
function:	main
parameter: int argc, char **argv
******************************************************************************/
int main(int argc, char **argv)
{
   while (argc > 0)
   {
      const char *program = shift_args(&argc, &argv);
      if (argc <= 0)
      {
         fprintf(stderr, "ERROR: no arguments provided\n");
         return 1;
      }
      const char *action = shift_args(&argc, &argv);
      if (strcmp(action, "list") == 0)
      {
         m_action = LIST;
         break;
      }
      else if (strcmp(action, "get") == 0)
      {
         m_action = GET;
         if (argc <= 0)
         {
            fprintf(stderr, "ERROR: no feature is provided for action `%s`\n", action);
            return 1;
         }
      }
      else if (strcmp(action, "set") == 0)
      {
         m_action = SET;
         if (argc <= 0)
         {
            fprintf(stderr, "ERROR: no feature is provided for action `%s`\n", action);
            return 1;
         }
      }
      else if (strcmp(action, "help") == 0)
      {
         usage(program);
         return 0;
      }
      else
      {
         fprintf(stderr, "ERROR: unrecognised action `%s`\n", action);
         return 1;
      }

      const char *target_or_feature = shift_args(&argc, &argv);

      long lnum;
      char *end;

      lnum = strtol(target_or_feature, &end, 10); // 10 specifies base-10
      if (!(end == target_or_feature))
      { // if at least one character was converted these pointers are unequal
         if ((lnum < 1) || (lnum > 32))
         {
            fprintf(stderr, "ERROR: target `%ld` out of range\n", lnum);
            return 1;
         }
         m_target = (uint16_t)lnum;
         target_or_feature = shift_args(&argc, &argv);
      }
      else
      {
         if (strcmp(target_or_feature, "all") == 0)
         {
            m_target = ALL_TARGETS;
            target_or_feature = shift_args(&argc, &argv);
         }
      }

      if (strcmp(target_or_feature, "brightness") == 0)
      {
         m_feature = BRIGHTNESS;
      }
      else if (strcmp(target_or_feature, "contrast") == 0)
      {
         m_feature = CONTRAST;
      }
      else if (strcmp(target_or_feature, "volume") == 0)
      {
         m_feature = VOLUME;
      }
      else
      {
         fprintf(stderr, "ERROR: unrecognised feature `%s`\n", target_or_feature);
         return 1;
      }

      if (m_action == GET)
      {
         break;
      }

      if ((argc <= 0) && (m_action == SET))
      {
         fprintf(stderr, "ERROR: no value is provided for feature `%s`\n", target_or_feature);
         return 1;
      }

      if (m_action == SET)
      {
         const char *value = shift_args(&argc, &argv);

         long lnum;
         char *end;

         lnum = strtol(value, &end, 10); // 10 specifies base-10

         if (end == value)
         { // if no characters were converted these pointers are equal
            fprintf(stderr, "ERROR: invalid value `%s`\n", value);
            return 1;
         }

         if (strcmp(end, "+") == 0)
         {
            m_change = 1;
         }
         else if (strcmp(end, "-") == 0)
         {
            m_change = -1;
         }

         if ((lnum < 0) || (lnum > 100))
         {
            fprintf(stderr, "ERROR: value `%ld` out of range\n", lnum);
            return 1;
         }
         m_value = (uint16_t)lnum;
      }
   }

   DDCA_Status ok = 0;
   DDCA_Status rc;
   DDCA_Display_Ref dref;
   DDCA_Display_Handle dh = NULL;
   DDCA_Display_Info_List *dlist = NULL;
   ddca_get_display_info_list2(
       false, // don't include invalid displays
       &dlist);
   if (dlist->ct == 0)
   {
      fprintf(stderr, "ERROR: No valid display detected\n");
      return 1;
   }

   if (m_action == LIST)
   {
      for (int ndx = 0; ndx < dlist->ct; ndx++)
      {
         dref = dlist->info[ndx].dref;
         if (dref)
         {
            printf("Found display:\n");
            ddca_report_display_by_dref(dref, 1);
         }
      }
      return 0;
   }

   int lb = 0;
   int ub = dlist->ct;

   if (!(m_target == ALL_TARGETS))
   {
      lb = m_target - 1;
      ub = m_target;
   }

   for (int ndx = lb; ndx < ub; ndx++)
   {
      DDCA_Display_Info *dinfo = &dlist->info[ndx];
      dref = dinfo->dref;
      rc = ddca_open_display2(dref, false, &dh);
      if (rc != 0)
      {
         fprintf(stderr, "ERROR: unable to open display `%d`\n", ndx + 1);
         continue;
      }
      if (m_action == GET)
      {
         uint16_t cur_val = get_value(dh, m_feature);
         if (!(cur_val < 0))
         {
            fprintf(stdout, "%d\n", cur_val);
         }
      }
      else if (m_action == SET)
      {
         if (!m_change)
         {
            ok = set_value(dh, m_feature, m_value);
         }
         else
         {
            uint16_t cur_val = get_value(dh, m_feature);
            if (!(cur_val < 0))
            {
               int testval = (int)cur_val + (m_change * m_value);
               if (testval < 0 || testval > 100)
               {
                  fprintf(stderr, "ERROR: value `%d` is out of range, current value is `%d`\n", testval, cur_val);
                  ok = 1;
               }
               else
               {
                  m_value = testval;
                  ok = set_value(dh, m_feature, m_value);
               }
            }
         }
      }

      rc = ddca_close_display(dh);
      if (rc != 0)
      {
         fprintf(stderr, "ERROR: unable to close display `%d`\n", ndx + 1);
      }
      dh = NULL;
   }
   ddca_free_display_info_list(dlist);
   return (rc || ok);
}
