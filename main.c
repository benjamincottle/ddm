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
#define BRIGHTNESS 0x10
#define CONTRAST 0x12
#define VOLUME 0x62

#define DDC_ERRMSG(function_name, status_code)     \
   do                                              \
   {                                               \
      printf("(%s) %s() returned %d (%s): %s\n",   \
             __func__, function_name, status_code, \
             ddca_rc_name(status_code),            \
             ddca_rc_desc(status_code));           \
   } while (0)

uint16_t m_action;
uint16_t m_target;
uint16_t m_value;
int m_change = 0;


/******************************************************************************
function:	get_value
parameter: DDCA_Display_Handle dh, DDCA_Vcp_Feature_Code m_target
******************************************************************************/
uint16_t get_value(DDCA_Display_Handle dh, DDCA_Vcp_Feature_Code m_target)
{
   DDCA_Status ddcrc0;
   DDCA_Non_Table_Vcp_Value valrec;
   ddcrc0 = ddca_get_non_table_vcp_value(
       dh,
       m_target,
       &valrec);
   if (ddcrc0 != 0)
   {
      DDC_ERRMSG("ddca_get_non_table_vcp_value", ddcrc0);
      return -1;
   }
   return valrec.sh << 8 | valrec.sl;
}

/******************************************************************************
function:	set_value
parameter: DDCA_Display_Handle dh, DDCA_Vcp_Feature_Code m_target, uint16_t m_value
******************************************************************************/
DDCA_Status set_value(DDCA_Display_Handle dh, DDCA_Vcp_Feature_Code m_target, uint16_t m_value)
{
   uint8_t hi_byte = m_value >> 8;
   uint8_t lo_byte = m_value & 0xff;
   DDCA_Status ddcrc = ddca_set_non_table_vcp_value(dh, m_target, hi_byte, lo_byte);
   if (ddcrc == DDCRC_VERIFY)
   {
      fprintf(stderr, "ERROR: Value verification failed.\n");
   }
   else if (ddcrc != 0)
   {
      DDC_ERRMSG("ddca_set_non_table_vcp_value", ddcrc);
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
   fprintf(stderr, "Usage: %s <ACTION> <TARGET> [PERCENTAGE][+-]\n", program);
   fprintf(stderr, "\nACTION (Required):\n");
   fprintf(stderr, "    get:           get a value\n");
   fprintf(stderr, "    set:           set a value\n");
   fprintf(stderr, "    help:           display usage\n");
   fprintf(stderr, "\nTARGET (Required):\n");
   fprintf(stderr, "    brightness:    get/set brightness\n");
   fprintf(stderr, "    contrast:      get/set contrast\n");
   fprintf(stderr, "    volume:        get/set volume\n");
   fprintf(stderr, "\nPERCENTAGE (Required for `set` ACTION):\n");
   fprintf(stderr, "    0 .. 100[+-]   optional trailing `+` or `-` indicates\n");
   fprintf(stderr, "                   an incremental change");
   fprintf(stderr, "\nExamples:\n");
   fprintf(stderr, "    $ %s get brightness\n", program);
   fprintf(stderr, "    $ %s set contrast 60\n", program);
   fprintf(stderr, "    $ %s set volume 10+\n", program);
}

/******************************************************************************
function:	main
parameter: int argc, char **argv
******************************************************************************/
int main(int argc, char **argv)
{
   const char *program = shift_args(&argc, &argv);
   if (argc == 0)
   {
      fprintf(stderr, "ERROR: no arguments provided\n");
      return 1;
   }

   while (argc > 0)
   {
      const char *action = shift_args(&argc, &argv);
      if (strcmp(action, "get") == 0)
      {
         m_action = GET;
         if (argc <= 0)
         {
            fprintf(stderr, "ERROR: no target is provided for action `%s`\n", action);
            return 1;
         }
      }
      else if (strcmp(action, "set") == 0)
      {
         m_action = SET;
         if (argc <= 0)
         {
            fprintf(stderr, "ERROR: no target is provided for action `%s`\n", action);
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
      const char *target = shift_args(&argc, &argv);
      if (strcmp(target, "brightness") == 0)
      {
         m_target = BRIGHTNESS;
      }
      else if (strcmp(target, "contrast") == 0)
      {
         m_target = CONTRAST;
      }
      else if (strcmp(target, "volume") == 0)
      {
         m_target = VOLUME;
      }
      else
      {
         fprintf(stderr, "ERROR: unrecognised target `%s`\n", target);
         return 1;         
      }

      if ((argc <= 0) && (m_action == SET))
      {
         fprintf(stderr, "ERROR: no value is provided for target `%s`\n", target);
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
   DDCA_Display_Handle dh = NULL; // initialize to avoid clang analyzer warning

   int MAX_DISPLAYS = 1; // limit the number of displays

   DDCA_Display_Info_List *dlist = NULL;
   ddca_get_display_info_list2(
       false, // don't include invalid displays
       &dlist);
   for (int ndx = 0; ndx < dlist->ct && ndx < MAX_DISPLAYS; ndx++)
   {
      DDCA_Display_Info *dinfo = &dlist->info[ndx];
      dref = dinfo->dref;
      rc = ddca_open_display2(dref, false, &dh);
      if (rc != 0)
      {
         DDC_ERRMSG("ddca_open_display", rc);
         continue;
      }
      if (m_action == GET)
      {
         uint16_t cur_val = get_value(dh, m_target);
         if (!(cur_val < 0))
         {
            fprintf(stdout, "%d\n", cur_val);
         }
      }
      else if (m_action == SET)
      {
         if (!m_change)
         {
            ok = set_value(dh, m_target, m_value);
         }
         else
         {
            uint16_t cur_val = get_value(dh, m_target);
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
                  ok = set_value(dh, m_target, m_value);
               }
            }
         }
      }

      rc = ddca_close_display(dh);
      if (rc != 0)
      {
         DDC_ERRMSG("ddca_close_display", rc);
      }
      dh = NULL;
   }
   ddca_free_display_info_list(dlist);
   return (rc || ok);
}